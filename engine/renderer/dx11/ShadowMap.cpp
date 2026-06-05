#include "ShadowMap.h"
#include "core/Logger.h"

namespace FaluEngine
{
	bool ShadowMap::create(ID3D11Device* device, uint32_t size)
	{
		m_size = size;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = size;
		td.Height = size;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
		if (FAILED(hr))
		{
			LOG_ERROR("ShadowMap: CreateTexture2D failed");
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		hr = device->CreateDepthStencilView(m_texture.Get(), &dsvDesc, &m_dsv);
		if (FAILED(hr))
		{
			LOG_ERROR("ShadowMap: CreateDepthStencilView failed");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
		if (FAILED(hr))
		{
			LOG_ERROR("ShadowMap: CreateShaderResourceView failed");
			return false;
		}

		LOG_INFO("ShadowMap created ({}x{})", size, size);
		return true;
	}

	void ShadowMap::release()
	{
		m_texture.Reset();
		m_dsv.Reset();
		m_srv.Reset();
	}

	void ShadowMap::bindForWrite(ID3D11DeviceContext* context)
	{
		ID3D11RenderTargetView* nullRTV = nullptr;
		context->OMSetRenderTargets(1, &nullRTV, m_dsv.Get());

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(m_size);
		vp.Height = static_cast<float>(m_size);
		vp.MaxDepth = 1.0f;
		context->RSSetViewports(1, &vp);
	}

	void ShadowMap::bindForRead(ID3D11DeviceContext* context, uint32_t slot)
	{
		context->PSSetShaderResources(slot, 1, m_srv.GetAddressOf());
	}

	void ShadowMap::unbind(ID3D11DeviceContext* context, uint32_t slot)
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->PSSetShaderResources(slot, 1, &nullSRV);
	}

	void ShadowMap::clear(ID3D11DeviceContext* context)
	{
		context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}
