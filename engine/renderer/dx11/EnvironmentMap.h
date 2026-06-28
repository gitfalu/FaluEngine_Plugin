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
	class EnvironmentMap
	{
	public:
		bool create(ID3D11Device* device, uint32_t size = 512, uint32_t mipLevels = 1);
		void release();

		[[nodiscard]] ID3D11RenderTargetView* getFaceRTV(int face,uint32_t mip = 0) const noexcept
		{
			return m_faceRTVs[mip][face].Get();
		}

		[[nodiscard]] ID3D11ShaderResourceView* getSRV() const noexcept { return m_srv.Get(); }

		[[nodiscard]] uint32_t getSize() const noexcept { return m_size;}
		[[nodiscard]] uint32_t getMipLevels() const noexcept { return m_mipLevels; }
		[[nodiscard]] bool isValid() const noexcept { return m_srv != nullptr; }

		static glm::mat4 getFaceViewMatrix(int face);
		static glm::mat4 getCubeProjectionMatrix();

	private:
		ComPtr<ID3D11Texture2D> m_cubeTexture;
		ComPtr<ID3D11RenderTargetView> m_faceRTVs[5][6];
		ComPtr<ID3D11ShaderResourceView> m_srv;
		uint32_t m_size = 512;
		uint32_t m_mipLevels = 1;
	};
}
