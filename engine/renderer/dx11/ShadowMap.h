#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace FaluEngine
{

	class ShadowMap
	{
	public:
		bool create(ID3D11Device* device, uint32_t size = 2048);
		void release();

		void bindForWrite(ID3D11DeviceContext* context);
		void bindForRead(ID3D11DeviceContext* context, uint32_t slot);

		void unbind(ID3D11DeviceContext* context, uint32_t slot);

		void clear(ID3D11DeviceContext* context);

		[[nodiscard]] ID3D11ShaderResourceView* getSRV() const noexcept { return m_srv.Get(); }
		[[nodiscard]] uint32_t getSize() const noexcept { return m_size; }
		[[nodiscard]] bool isValid() const noexcept { return m_dsv != nullptr; }

		glm::mat4 lightView = glm::mat4(1.0f);
		glm::mat4 lightProjection = glm::mat4(1.0f);

		[[nodiscard]] glm::mat4 getLightSpaceMatrix() const {
			return lightProjection * lightView;
		}
	private:
		ComPtr<ID3D11Texture2D> m_texture;
		ComPtr<ID3D11DepthStencilView> m_dsv;
		ComPtr<ID3D11ShaderResourceView> m_srv;

		uint32_t m_size = 2048;
	};
}
