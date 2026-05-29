#include "RenderTexture.h"
#include "core/Logger.h"



namespace FaluEngine
{


	bool RenderTexture::create(ID3D11Device* device, uint32_t width, uint32_t height)
	{
		return createInternal(device,width,height);
	}

	void RenderTexture::resize(ID3D11Device* device, uint32_t width, uint32_t height)
	{
		if (width == m_width && height == m_height) return;
		createInternal(device, width, height);
	}

	void RenderTexture::release()
	{
		m_texture.Reset();
		m_rtv.Reset();
		m_srv.Reset();
		m_depthBuffer.Reset();
		m_dsv.Reset();
	}

	void RenderTexture::bindAsRenderTarget(ID3D11DeviceContext* context)
	{
		context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(m_width);
		vp.Height = static_cast<float>(m_height);
		vp.MaxDepth = 1.0f;
		context->RSSetViewports(1, &vp);
	}

	void RenderTexture::clear(ID3D11DeviceContext* context, const float color[4])
	{
		context->ClearRenderTargetView(m_rtv.Get(), color);
		context->ClearDepthStencilView(m_dsv.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,1.0f,0);
	}

	bool RenderTexture::createInternal(ID3D11Device* device, uint32_t width, uint32_t height)
	{
		release();
		m_width = width;
		m_height = height;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = width;
		td.Height = height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
		if (FAILED(hr)) { LOG_ERROR("RenderTexture: CreateTexture2D failed"); return false; }

		hr = device->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
		if(FAILED(hr)){ LOG_ERROR("RenderTexture: CreateRenderTargetView failed"); return false; }

		hr = device->CreateShaderResourceView(m_texture.Get(), nullptr, &m_srv);
		if (FAILED(hr)) { LOG_ERROR("RenderTexture: CreateShaderResourceView failed"); return false; }

		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = width;
		dd.Height = height;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = device->CreateTexture2D(&dd, nullptr, &m_depthBuffer);
		if (FAILED(hr)) { LOG_ERROR("RenderTexture: CreateDepthBuffer failed"); return false; }

		hr = device->CreateDepthStencilView(m_depthBuffer.Get(), nullptr, &m_dsv);
		if (FAILED(hr)) { LOG_ERROR("RenderTexture: CreateDSV failed"); return false; }

		return true;
	}

}