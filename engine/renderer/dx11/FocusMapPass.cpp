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

		D3D11_INPUT_ELEMENT_DESC staticLayout[] = {
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEINDICES",0,DXGI_FORMAT_R32G32B32A32_SINT,0,offsetof(SkinnedVertex,boneIndices),D3D11_INPUT_PER_VERTEX_DATA,0},
			{"BONEWEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(SkinnedVertex,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		//if(FAILED(device->CreateInputLayout(staticLayout,ARRAYSIZE(staticLayout),)))

		return true;
	}

	bool FocusMapPass::analyze(Scene& scene, const FocusSettings& settings, uint32_t gridW, uint32_t gridH, uint32_t uiRefW, uint32_t uiRefH, FocusMapResult& out)
	{
		return false;
	}

	void FocusMapPass::updatePreview(const FocusMapResult& result, const FocusSettings& settings)
	{
	}

	bool FocusMapPass::ensureTargets(uint32_t w, uint32_t h)
	{
		return false;
	}

	void FocusMapPass::drawMeshes(Scene& scene, const FocusSettings& settings, const glm::mat4& view, const glm::mat4& proj, FocusMapResult& out)
	{
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
