#include "FocusMapPass.h"
#include "core/Logger.h"
#include "FaluEngine/Component.h"
#include "asset/loaders/MeshLoader.h"
#include "renderer/dx11/DX11Renderer.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace FaluEngine
{
	namespace
	{
		const char* kHLSL = R"HLSL(
			cbuffer TransformCB : register (b0) { float4x4 mvp; };
			cbuffer IdCB : register(b1) { float4 idColor; };
			cbuffer SkinCB : register(b4) { float4x4 bones[128]; };

			struct StaticIn { float3 pos : POSITION; };
			float4 VSStatic(StaticIn i) : SV_POSITION
			{
				return mul(float4(i.pos, 1.0),mvp);
			}

			struct SkinIn { float3 pos : POSITION; int4 idx : BONEINDICES; float4 w : BONEWEIGHTS; };
			float4 VSSkinned(SkinIn i) : SV_POSITION
			{
				float4 p = float4( i.pos,1.0);
				float4 s = float4(0,0,0,0);
				[unroll] for( int k = 0;k < 4;++k)
				{
					if(i.idx[k] >= 0 && i.w[k] > 0.0)
						s += i.w[k] * mul(p,bones[i.idx[k]]);
				}
				if(s.w < 0.5) s = p;
				return mul(float4(s.xyz,1.0),mvp);
			}
			float4 PS() : SV_Target { return idColor; }
		)HLSL";

		bool compile(const char* entry, const char* profile, Microsoft::WRL::ComPtr<ID3DBlob>& blob)
		{
			Microsoft::WRL::ComPtr<ID3DBlob> err;
			HRESULT hr = D3DCompile(kHLSL, std::strlen(kHLSL), "FocusMap", nullptr, nullptr,
				entry, profile, 0, 0, &blob, &err);
			if (FAILED(hr))
			{
				LOG_ERROR("[FocusMap] {} compile error: {}", entry,
					err ? static_cast<const char*>(err->GetBufferPointer()) : "unknown");
				return false;
			}
			return true;
		}

		int categoryIdOf(entt::registry& reg, entt::entity e, const FocusSettings& s)
		{
			std::string name = "Untagged";
			if (auto* tag = reg.try_get<TagComponent>(e)) name = tag->category;
			int id = s.findId(name);
			if (id == 0) id = s.findId("Untagged");
			return id;
		}
	}

	
	bool FocusMapPass::init(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		m_device = device;
		m_context = context;
		m_ready = false;
		if (!device || !context) return false;

		ComPtr<ID3DBlob> vsStaticBlob, vsSkinBlob, psBlob;
		if (!compile("VSStatic", "vs_5_0", vsStaticBlob) ||
			!compile("VSSkinned", "vs_5_0", vsSkinBlob) ||
			!compile("PS", "ps_5_0", psBlob))
			return false;

		if (FAILED(device->CreateVertexShader(vsStaticBlob->GetBufferPointer(), vsStaticBlob->GetBufferSize(), nullptr, &m_vsStatic)) ||
			FAILED(device->CreateVertexShader(vsSkinBlob->GetBufferPointer(), vsSkinBlob->GetBufferSize(), nullptr, &m_vsSkinned)) ||
			FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps)))
		{
			LOG_ERROR("[FocusMap] CreateShader failed");
			return false;
		}

		D3D11_INPUT_ELEMENT_DESC staticLayout[] =
		{
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		D3D11_INPUT_ELEMENT_DESC skinLayout[] = {
			{"POSITION",	0,DXGI_FORMAT_R32G32B32_FLOAT,		0,offsetof(SkinnedVertex,	position),		D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEINDICES",	0,DXGI_FORMAT_R32G32B32A32_SINT,	0,offsetof(SkinnedVertex,	boneIndices),	D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEWEIGHTS",	0,DXGI_FORMAT_R32G32B32A32_FLOAT,	0,offsetof(SkinnedVertex,	boneWeights),	D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		if (FAILED(device->CreateInputLayout(staticLayout, ARRAYSIZE(staticLayout),
				vsStaticBlob->GetBufferPointer(), vsStaticBlob->GetBufferSize(), &m_layoutStatic)) ||
			FAILED(device->CreateInputLayout(skinLayout, ARRAYSIZE(skinLayout),
				vsSkinBlob->GetBufferPointer(), vsSkinBlob->GetBufferSize(), &m_layoutSkinned)))
		{
			LOG_ERROR("[FocusMap] CreateInputLayout failed");
			return false;
		}

		auto makeCB = [&](UINT bytes, ComPtr<ID3D11Buffer>& dst)
			{
				D3D11_BUFFER_DESC bd = {};
				bd.ByteWidth = bytes;
				bd.Usage = D3D11_USAGE_DYNAMIC;
				bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &dst));
			};

		if (!makeCB(sizeof(glm::mat4), m_cbTransform) ||
			!makeCB(sizeof(float) * 4, m_cbId) ||
			!makeCB(sizeof(glm::mat4) * MAX_BONES, m_cbSkin))
		{
			LOG_ERROR("[FocusMap] CreateBuffer failed");
			return false;
		}

		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.DepthClipEnable = TRUE;
		device->CreateRasterizerState(&rd, &m_rasterizer);

		D3D11_DEPTH_STENCIL_DESC dd = {};
		dd.DepthEnable = TRUE;
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dd.DepthFunc = D3D11_COMPARISON_LESS;
		device->CreateDepthStencilState(&dd, &m_depthState);

		m_ready = true;
		LOG_INFO("[FucusMap] initialized");
		return true;
	}

	bool FocusMapPass::analyze(Scene& scene, const FocusSettings& settings, uint32_t gridW, uint32_t gridH, uint32_t uiRefW, uint32_t uiRefH, FocusMapResult& out)
	{
		if (!m_ready || gridW == 0 || gridH == 0) return false;

		auto& reg = scene.registry();
		const Camera* cam = nullptr;
		{
			auto camView = reg.view<CameraComponent, TransformComponent>();
			for (auto e : camView)
			{
				auto& c = camView.get<CameraComponent>(e);
				if (c.isPrimary) { cam = &c.camera; break; }
			}
		}

		if (!cam) return false;

		const glm::mat4 view = cam->getView();
		const glm::mat4 proj = glm::perspectiveLH(glm::radians(cam->getFovDeg()),
			static_cast<float>(gridW) / static_cast<float>(gridH), cam->getNearClip(), cam->getFarClip());

		if (!ensureTargets(gridW, gridH)) return false;

		out.width = gridW; out.height = gridH;
		out.category.assign(static_cast<size_t>(gridW) * gridH, 0);
		out.entityIndex.assign(static_cast<size_t>(gridW) * gridH, 0);
		out.entities.clear();
		out.categoryStats.clear();
		out.entityStats.clear();

		ID3D11RenderTargetView* oldRTV = nullptr;
		ID3D11DepthStencilView* oldDSV = nullptr;
		m_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
		UINT vpCount = 1;
		D3D11_VIEWPORT oldVP = {};
		m_context->RSGetViewports(&vpCount, &oldVP);

		drawMeshes(scene, settings, view, proj, out);
		readback(out);

		m_context->OMSetRenderTargets(1, &oldRTV, oldDSV);
		if (vpCount > 0) m_context->RSSetViewports(1, &oldVP);
		if (oldRTV) oldRTV->Release();
		if (oldDSV) oldDSV->Release();
		
		compositeUI(scene, settings, uiRefW, uiRefH, out);
		computeStats(scene, settings, out);
		return true;
	}

	void FocusMapPass::updatePreview(const FocusMapResult& result, const FocusSettings& settings)
	{
		if (!m_device || result.width == 0 || result.height == 0) return;
		
		if (!m_previewTex || m_previewW != result.width || m_previewH != result.height)
		{
			m_previewTex.Reset(); m_previewSRV.Reset();
			D3D11_TEXTURE2D_DESC td = {};
			td.Width = result.width; td.Height = result.height;
			td.MipLevels = 1; td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_previewTex)) ||
				FAILED(m_device->CreateShaderResourceView(m_previewTex.Get(), nullptr, &m_previewSRV)))
				return;
			m_previewW = result.width; m_previewH = result.height;
		}

		auto pack = [](float r, float g, float b) ->uint32_t {
			auto q = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return q(r) | (q(g) << 8) | (q(b) << 16) | (255u << 24);
			};
		std::vector<uint32_t> palette(settings.categories.size() + 1);
		palette[0] = pack(0.08f, 0.08f, 0.10f);
		for (size_t i = 0; i < settings.categories.size(); ++i)
			palette[i + 1] = pack(settings.categories[i].color.r, settings.categories[i].color.g, settings.categories[i].color.b);

		std::vector<uint32_t> pixels(result.category.size());
		for (size_t i = 0; i < pixels.size(); ++i)
		{
			pixels[i] = palette[(std::min)(static_cast<size_t>(result.category[i]), palette.size() - 1)];
		}

		m_context->UpdateSubresource(m_previewTex.Get(), 0, nullptr, pixels.data(), result.width * 4, 0);
	}

	bool FocusMapPass::ensureTargets(uint32_t w, uint32_t h)
	{
		if (m_rt && m_w == w && m_h == h) return true;
		m_rt.Reset(); m_rtv.Reset(); m_depth.Reset(); m_dsv.Reset(); m_staging.Reset();

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = w; td.Height = h;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET;
		if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_rt)) ||
			FAILED(m_device->CreateRenderTargetView(m_rt.Get(),nullptr,&m_rtv)))
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC dd = td;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if(FAILED(m_device->CreateTexture2D(&dd,nullptr,&m_depth)) || 
			FAILED(m_device->CreateDepthStencilView(m_depth.Get(), nullptr, &m_dsv)))
		{
			return false; 
		}

		D3D11_TEXTURE2D_DESC sd = td;
		sd.Usage = D3D11_USAGE_STAGING;
		sd.BindFlags = 0;
		sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		if (FAILED(m_device->CreateTexture2D(&sd, nullptr, &m_staging)))
			return false;

		m_w = w; m_h = h;
		return true;
	}

	void FocusMapPass::drawMeshes(Scene& scene, const FocusSettings& settings, const glm::mat4& view, const glm::mat4& proj, FocusMapResult& out)
	{
		auto& reg = scene.registry();

		const float clear[4] = { 0.0f,0.0f,0.0f,0.0f };
		m_context->ClearRenderTargetView(m_rtv.Get(), clear);
		m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		ID3D11RenderTargetView* rtv = m_rtv.Get();
		m_context->OMSetRenderTargets(1, &rtv, m_dsv.Get());

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(m_w);
		vp.Height = static_cast<float>(m_h);
		vp.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &vp);

		m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		m_context->OMSetDepthStencilState(m_depthState.Get(), 0);
		m_context->RSSetState(m_rasterizer.Get());
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_context->PSSetShader(m_ps.Get(), nullptr, 0);
		m_context->VSSetConstantBuffers(0, 1, m_cbTransform.GetAddressOf());
		m_context->PSSetConstantBuffers(1, 1, m_cbId.GetAddressOf());
		m_context->VSSetConstantBuffers(4, 1, m_cbSkin.GetAddressOf());

		auto upload = [&](ID3D11Buffer* cb, const void* data, size_t bytes)
		{
			D3D11_MAPPED_SUBRESOURCE ms = {};
			if (SUCCEEDED(m_context->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
			{
				std::memcpy(ms.pData, data, bytes);
				m_context->Unmap(cb, 0);
			}
		};

		auto meshView = reg.view<MeshComponent, TransformComponent>();
		for (auto e : meshView)
		{
			auto& mesh = meshView.get<MeshComponent>(e);
			auto& tr = meshView.get<TransformComponent>(e);
			if (!mesh.visible || !mesh.cachedMesh) continue;
			MeshAsset* ma = mesh.cachedMesh.get();
			if (!ma->vertexBuffer || !ma->indexBuffer) continue;
			if (out.entities.size() >= 65535) break;

			const int catId = categoryIdOf(reg, e, settings);
			if (catId == 0) continue;

			out.entities.push_back(e);
			const uint32_t idx = static_cast<uint32_t>(out.entities.size());
			const float idColor[4] = {
				static_cast<float>(catId) / 255.0f,
				static_cast<float>(idx & 0xFF) / 255.0f,
				static_cast<float>((idx >> 8) & 0xFF) / 255.0f,
				1.0f
			};
			upload(m_cbId.Get(), idColor, sizeof(idColor));

			const glm::mat4 mvp = glm::transpose(proj * view * tr.worldMatrix);
			upload(m_cbTransform.Get(), &mvp, sizeof(mvp));

			const bool skinned = ma->hasSkeleton;
			if (skinned)
			{
				glm::mat4 bones[MAX_BONES];
				for (auto& b : bones) b = glm::mat4(1.0f);
				if(auto* anim = reg.try_get<AnimatorComponent>(e))
				{
					const size_t n = (std::min)(anim->boneMatrices.size(), static_cast<size_t>(MAX_BONES));
					for (size_t i = 0; i < n; ++i)
					{
						bones[i] = glm::transpose(anim->boneMatrices[i]);
					}
				}
				upload(m_cbSkin.Get(), bones, sizeof(bones));
				m_context->VSSetShader(m_vsSkinned.Get(), nullptr, 0);
				m_context->IASetInputLayout(m_layoutSkinned.Get());
			}
			else
			{
				m_context->VSSetShader(m_vsStatic.Get(), nullptr, 0);
				m_context->IASetInputLayout(m_layoutStatic.Get());
			}

			ID3D11Buffer* vb = ma->vertexBuffer.Get();
			UINT stride = skinned ? sizeof(SkinnedVertex) : sizeof(Vertex), offset = 0;
			m_context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
			m_context->IASetIndexBuffer(ma->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			for(const auto& sub : ma->subMeshes)
				m_context->DrawIndexed(sub.indexCount, sub.indexOffset, 0);
		}
	}

	void FocusMapPass::readback(FocusMapResult& out)
	{
		m_context->CopyResource(m_staging.Get(), m_rt.Get());
		D3D11_MAPPED_SUBRESOURCE ms = {};
		if(FAILED(m_context->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &ms)))
		{
			LOG_ERROR("[FocusMap] Map(string) failed");
			return;
		}
		for (uint32_t y = 0; y < m_h; ++y)
		{
			const uint8_t* row = static_cast<const uint8_t*>(ms.pData) + static_cast<size_t>(y) * ms.RowPitch;
			for (uint32_t x = 0; x < m_w; ++x)
			{
				const uint8_t* px = row + static_cast<size_t>(x) * 4;
				const size_t i = static_cast<size_t>(y) * m_w + x;
				out.category[i] = px[0];
				out.entityIndex[i] = static_cast<uint16_t>(px[1] | (px[2] << 8));
			}
		}
		m_context->Unmap(m_staging.Get(), 0);
	}

	void FocusMapPass::compositeUI(Scene& scene, const FocusSettings& settings, uint32_t uiRefW, uint32_t uiRefH, FocusMapResult& out)
	{
		if (uiRefW == 0 || uiRefH == 0) return;
		auto& reg = scene.registry();

		bool anyCanvas = false;
		for (auto e : reg.view<CanvasComponent>())
			if (reg.get<CanvasComponent>(e).enabled) { anyCanvas = true; break; }
		if (!anyCanvas) return;

		const float w = static_cast<float>(uiRefW), h = static_cast<float>(uiRefH);
		const float gw = static_cast<float>(out.width), gh = static_cast<float>(out.height);

		auto imageView = reg.view<ImageComponent, RectTransformComponent>();
		for (auto e : imageView)
		{
			auto& img = imageView.get<ImageComponent>(e);
			auto& rt = imageView.get<RectTransformComponent>(e);
			if (!img.visible || img.color.a < 0.02f) continue;
			if (out.entities.size() >= 65535) break;

			const int catId = categoryIdOf(reg, e, settings);
			if (catId == 0) continue;

			out.entities.push_back(e);
			const uint16_t idx = static_cast<uint16_t>(out.entities.size());

			const glm::vec2 half = rt.computedSize * 0.5f;
			const float ang = glm::radians(rt.rotation);
			const float c = std::cos(ang), s = std::sin(ang);
			const float ex = std::fabs(c) * half.x + std::fabs(s) * half.y;
			const float ey = std::fabs(s) * half.x + std::fabs(c) * half.y;

			const int x0 = (std::max)(0, static_cast<int>(std::floor((rt.computedPosition.x - ex) / w * gw + 0.5f * gw)));
			const int x1 = (std::min)(static_cast<int>(out.width) - 1, static_cast<int>(std::ceil((rt.computedPosition.x + ex) / w * gw + 0.5f * gw)));
			const int y0 = (std::max)(0, static_cast<int>(std::floor((0.5f - (rt.computedPosition.y + ey) / h) * gh)));
			const int y1 = (std::min)(static_cast<int>(out.height) - 1, static_cast<int>(std::ceil((0.5f - (rt.computedPosition.y - ey) / h) * gh)));

			for (int y = y0; y <= y1; ++y)
			{
				for (int x = x0; x <= x1; ++x)
				{
					const float ux = ((x + 0.5f) / gw - 0.5f) * w;
					const float uy = (0.5f - (y + 0.5f) / gh) * h;
					const float dx = ux - rt.computedPosition.x;
					const float dy = uy - rt.computedPosition.y;
					const float lx = c * dx + s * dy;
					const float ly = -s * dx + c * dy;
					if (std::fabs(lx) <= half.x && std::fabs(ly) <= half.y)
					{
						const size_t i = static_cast<size_t>(y) * out.width + x;
						out.category[i] = static_cast<uint8_t>(catId);
						out.entityIndex[i] = idx;
					}
				}
			}
		}
	}

	void FocusMapPass::computeStats(Scene& scene, const FocusSettings& settings, FocusMapResult& out)
	{
		auto& reg = scene.registry();
		const size_t total = static_cast<size_t>(out.width) * out.height;
		if (total == 0) return;

		std::vector<uint32_t> catCount(settings.categories.size() + 1, 0);
		std::vector<uint32_t> entCount(out.entities.size() + 1, 0);
		for (size_t i = 0; i < total; ++i)
		{
			const size_t c = (std::min)(static_cast<size_t>(out.category[i]), catCount.size() - 1);
			catCount[c]++;
			const size_t e = out.entityIndex[i];
			if (e < entCount.size()) entCount[e]++;
		}

		out.categoryStats.resize(catCount.size());
		for (size_t id = 0; id < catCount.size(); ++id)
		{
			out.categoryStats[id].id = static_cast<int>(id);
			out.categoryStats[id].pixels = catCount[id];
			out.categoryStats[id].ratio = static_cast<float>(catCount[id]) / static_cast<float>(total);
		}

		for (size_t k = 0; k < out.entities.size(); ++k)
		{
			if (entCount[k + 1] == 0) continue;
			FocusEntityStat st;
			st.entity = out.entities[k];
			if (auto* tag = reg.try_get<TagComponent>(st.entity)) st.name = tag->name;
			st.categoryId = categoryIdOf(reg, st.entity, settings);
			st.pixels = entCount[k + 1];
			st.ratio = static_cast<float>(st.pixels) / static_cast<float>(total);
			out.entityStats.push_back(std::move(st));
		}
		std::sort(out.entityStats.begin(), out.entityStats.end(),
			[](const FocusEntityStat& a, const FocusEntityStat& b) {return a.pixels > b.pixels; });
	}

}
