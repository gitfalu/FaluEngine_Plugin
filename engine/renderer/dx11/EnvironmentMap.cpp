#include "EnvironmentMap.h"
#include "core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>

namespace FaluEngine
{
	bool EnvironmentMap::create(ID3D11Device* device, uint32_t size,uint32_t mipLevels)
	{
		release();
		m_size = size;
		m_mipLevels = mipLevels;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = size;
		td.Height = size;
		td.MipLevels = mipLevels;
		td.ArraySize = 6;
		td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_cubeTexture);
		if (FAILED(hr))
		{
			LOG_ERROR("EnvironmentMap: CreateTexture2D Failed");
			return false;
		}

		for (uint32_t mip = 0;mip < mipLevels; ++mip)
		{
			for (int face = 0; face < 6; ++face)
			{
				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
				rtvDesc.Format = td.Format;
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.MipSlice = mip;
				rtvDesc.Texture2DArray.FirstArraySlice = face;
				rtvDesc.Texture2DArray.ArraySize = 1;

				hr = device->CreateRenderTargetView(m_cubeTexture.Get(), &rtvDesc, &m_faceRTVs[mip][face]);
				if (FAILED(hr))
				{
					LOG_ERROR("EnvironmentMap: CreateRenderTargetView failed for (mip {}, face {})", mip,face);
					return false;
				}
			}
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = td.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = mipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;

		hr = device->CreateShaderResourceView(m_cubeTexture.Get(), &srvDesc, &m_srv);
		if (FAILED(hr))
		{
			LOG_ERROR("EnvironmenetMap: CreateShaderREsourceView failed");
			return false;
		}

		LOG_INFO("EnvironmentMap created ({}x{} x6)", size, size);
		return true;
	}

	void EnvironmentMap::release()
	{
		m_cubeTexture.Reset();
		for(auto& mipArr : m_faceRTVs)
			for (auto& rtv : mipArr) 
				rtv.Reset();
		m_srv.Reset();
	}
	glm::mat4 EnvironmentMap::getFaceViewMatrix(int face)
	{
		const glm::vec3 origin = { 0.0f,0.0f,0.0f };
		switch (face)
		{
		case 0:return glm::lookAtLH(origin, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));
		case 1:return glm::lookAtLH(origin, glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0));
		case 2:return glm::lookAtLH(origin, glm::vec3(0, 1, 0), glm::vec3(0, 0, -1));
		case 3:return glm::lookAtLH(origin, glm::vec3(0, -1, 0), glm::vec3(0, 0, 1));
		case 4:return glm::lookAtLH(origin, glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
		default: return glm::lookAtLH(origin, glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
		
		}
	}
	glm::mat4 EnvironmentMap::getCubeProjectionMatrix()
	{
		return glm::perspectiveLH(glm::radians(90.0f),1.0f,0.1f,10.0f);
	}
}
