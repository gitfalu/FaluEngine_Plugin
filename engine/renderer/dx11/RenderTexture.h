#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace FaluEngine
{

	class RenderTexture
	{
	public:
		bool create(ID3D11Device* device, uint32_t width, uint32_t height);
		void resize(ID3D11Device* device, uint32_t width, uint32_t height);
		void release();

		void bindAsRenderTarget(ID3D11DeviceContext* context);
		
		[[nodiscard]] ID3D11ShaderResourceView* getSRV() const noexcept { return m_srv.Get(); }

		[[nodiscard]] uint32_t getWidth() const noexcept { return m_width; }
		[[nodiscard]] uint32_t getHeight() const noexcept { return m_height; }
		[[nodiscard]] bool isValid() const noexcept { return m_rtv != nullptr; }

		void clear(ID3D11DeviceContext* context, const float color[4]);
	private:
		bool createInternal(ID3D11Device* device, uint32_t width, uint32_t height);

		ComPtr<ID3D11Texture2D> m_texture;
		ComPtr<ID3D11RenderTargetView> m_rtv;
		ComPtr<ID3D11ShaderResourceView> m_srv;
		ComPtr<ID3D11Texture2D> m_depthBuffer;
		ComPtr<ID3D11DepthStencilView> m_dsv;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
	};
}
