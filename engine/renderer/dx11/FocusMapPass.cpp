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
		if (!device || !context) return;

		ComPtr<ID3DBlob> vsStaticBlob, vsSkinBlob, psBlob;
		if (!compile("VSStatic", "vs_5_0", vsStaticBlob) ||
			!compile("VSSkinned", "vs_5_0", vsStaticBlob) ||
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
			{"POISTION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		D3D11_INPUT_ELEMENT_DESC skinLayout[] = {
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEINDICES",0,DXGI_FORMAT_R32G32B32A32_SINT,0,offsetof(SkinnedVertex,boneIndices),D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEWEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(SkinnedVertex,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0},
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

		CD3D11_RASTERIZER_DESC rd = {};
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
	}

	bool FocusMapPass::ensureTargets(uint32_t w, uint32_t h)
	{
		if (m_rt && m_w == w && m_h == h) return true;
		m_rt.Reset(); m_rtv.Reset(); m_depth.Reset(); m_dsv.Reset(); m_staging.Reset();

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = 2; td.Height = h;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET;
		if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_rt)) ||
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
		m_context->OMSetRenderTargets(1, &m_rtv, m_dsv.Get());


	}

	void FocusMapPass::readback(FocusMapResult& out)
	{
	}

	void FocusMapPass::compositeUI(Scene& scene, const FocusSettings& settings, uint32_t uiRefW, uint32_t uiRefH, FocusMapResult& out)
	{
	}

	void FocusMapPass::computeStats(Scene& scene, const FocusSettings& settings, FocusMapResult& out)
	{
	}

}
