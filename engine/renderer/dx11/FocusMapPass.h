#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <FaluEngine/ENgineExport.h>
#include <FaluEngine/FocusTypes.h>

namespace FaluEngine
{
	class Scene;

	class FALU_ENGINE_API FocusMapPass
	{
	public:
		bool init(ID3D11Device* device, ID3D11DeviceContext* context);

		bool analyze(Scene& scene, const FocusSettings& settings,
			uint32_t gridW, uint32_t gridH, uint32_t uiRefW, uint32_t uiRefH,
			FocusMapResult& out);

		void updatePreview(const FocusMapResult& result, const FocusSettings& settings);

		[[nodiscard]] ID3D11ShaderResourceView* getPreviewSRV() const noexcept { return m_previewSRV.Get(); }
		[[nodiscard]] bool isValid() const noexcept { return m_ready; }
	private:
		bool ensureTargets(uint32_t w, uint32_t h);
		void drawMeshes(Scene& scene, const FocusSettings& settings,
			const glm::mat4& view, const glm::mat4& proj, FocusMapResult& out);
		void readback(FocusMapResult& out);
		void compositeUI(Scene& scene, const FocusSettings& settings,
			uint32_t uiRefW, uint32_t uiRefH, FocusMapResult& out);
		void computeStats(Scene& scene, const FocusSettings& settings, FocusMapResult& out);

		template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

		ID3D11Device* m_device = nullptr;
		ID3D11DeviceContext* m_context = nullptr;
		bool m_ready = false;

		ComPtr<ID3D11VertexShader> m_vsStatic, m_vsSkinned;
		ComPtr<ID3D11PixelShader> m_ps;
		ComPtr<ID3D11InputLayout> m_layoutStatic, m_layoutSkinned;
		ComPtr<ID3D11Buffer> m_cbTransform, m_cbId, m_cbSkin;
		ComPtr<ID3D11RasterizerState> m_rasterizer;
		ComPtr<ID3D11DepthStencilState> m_depthState;

		ComPtr<ID3D11Texture2D> m_rt, m_depth, m_staging;
		ComPtr<ID3D11RenderTargetView> m_rtv;
		ComPtr<ID3D11DepthStencilView> m_dsv;
		uint32_t m_w = 0, m_h = 0;

		ComPtr<ID3D11Texture2D> m_previewTex;
		ComPtr<ID3D11ShaderResourceView> m_previewSRV;
		uint32_t m_previewW = 0, m_previewH = 0;
	};
}
