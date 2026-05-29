#include "DX11Renderer.h"
#include "core/Logger.h"
#include "scene/Scene.h"
#include "scene/Component.h"
#include "core/PathResolver.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

namespace FaluEngine {

bool DX11Renderer::init(void* windowHandle, uint32_t width, uint32_t height) {
    m_width  = width;
    m_height = height;
    HWND hwnd = static_cast<HWND>(windowHandle);

    if (!createDeviceAndSwapChain(hwnd))       return false;
    if (!createRenderTargetView())    return false;
    if (!createDepthStencilView())    return false;
    if (!createShaders(
        PathResolver::resolveStr("assets/shaders/Basic.vert.hlsl"),
        PathResolver::resolveStr("assets/shaders/Basic.pixel.hlsl"))) 
        return false;
    if (!createDefaultStates()) return false;

    updateViewport();

    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    m_projection = glm::perspectiveLH(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    LOG_INFO("DX11Renderer initialized ({}x{})", m_width, m_height);
    return true;
}

bool DX11Renderer::createDeviceAndSwapChain(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = m_width;
    sd.BufferDesc.Height                  = m_height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#ifdef ENGINE_DX11_DEBUG_LAYER
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, &featureLevel, &m_context
    );

    if (FAILED(hr)) {
        LOG_ERROR("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }
    return true;
}

bool DX11Renderer::createRenderTargetView() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv);
    return SUCCEEDED(hr);
}

bool DX11Renderer::createDepthStencilView() {
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width            = m_width;
    dd.Height           = m_height;
    dd.MipLevels        = 1;
    dd.ArraySize        = 1;
    dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage            = D3D11_USAGE_DEFAULT;
    dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_device->CreateTexture2D(&dd, nullptr, &m_depthBuffer);
    if (FAILED(hr)) return false;

    hr = m_device->CreateDepthStencilView(m_depthBuffer.Get(), nullptr, &m_dsv);
    return SUCCEEDED(hr);
}

bool DX11Renderer::createShaders(const std::string& vsPath, const std::string& psPath)
{
    if (!std::filesystem::exists(vsPath)) {
        LOG_ERROR("Vertex shader not found: {}", vsPath);
        return false;
    }

    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    UINT compileFlags = 0;
#ifdef ENGINE_DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        std::wstring(vsPath.begin(), vsPath.end()).c_str(),
        nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
        &vsBlob,&errorBlob
    );

    if (FAILED(hr)) {
        if(errorBlob)
            LOG_ERROR("VS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    if (!std::filesystem::exists(psPath)) {
        LOG_ERROR("Pixel shader not found: {}", psPath);
        return false;
    }

    hr = D3DCompileFromFile(
        std::wstring(psPath.begin(), psPath.end()).c_str(),
        nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
        &psBlob, &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob)
            LOG_ERROR("PS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,
        offsetof(Vertex,color),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,uv),}
    };

    hr = m_device->CreateInputLayout(
        layout, ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_inputLayout
    );

    if (FAILED(hr)) {
        LOG_ERROR("CreateInputLayout failed");
        return false;
    }

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(TransformCB);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbd, nullptr, &m_transformCB);
    if (FAILED(hr)) {
        LOG_ERROR("CreateBuffer (TransformCB) failed");
        return false;
    }

    D3D11_BUFFER_DESC mbd = {};
    mbd.ByteWidth = sizeof(MaterialCB);
    mbd.Usage = D3D11_USAGE_DYNAMIC;
    mbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&mbd, nullptr, &m_materialCB);

    if (FAILED(hr))
    {
        LOG_ERROR("CreateBuffer (MaterialCB) failed");
        return false;
    }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    m_device->CreateSamplerState(&sd, &m_samplerState);

    LOG_INFO("Shaders loaded: {} / {}", vsPath, psPath);
    return true;
}

bool DX11Renderer::createDefaultStates()
{
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    m_device->CreateRasterizerState(&rd, &m_rasterizerState);

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    m_device->CreateDepthStencilState(&dsd, &m_depthStencilState);

    return true;
}

void DX11Renderer::updateViewport()
{
    D3D11_VIEWPORT vp = {};

    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &vp);
}

void DX11Renderer::shutdown() {
    if (m_context) m_context->ClearState();
    LOG_INFO("DX11Renderer shutdown");
}

void DX11Renderer::beginFrame() {
    m_context->ClearRenderTargetView(m_rtv.Get(), m_clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());

    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    m_boundVB = nullptr;
    m_boundIB = nullptr;
}

void DX11Renderer::endFrame() {
    m_swapChain->Present(m_vsync ? 1 : 0, 0);
}

void DX11Renderer::renderScene(const Scene& /*scene*/) {
    // MeshComponent を持つエンティティを走査してドローコールを発行する
    // （RenderGraph 実装後にここを拡張する）
}

void DX11Renderer::onResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    m_width  = width;
    m_height = height;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    m_depthBuffer.Reset();

    m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    createRenderTargetView();
    createDepthStencilView();
    updateViewport();

    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    m_projection = glm::perspectiveLH(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
}

void DX11Renderer::drawMesh(const Vertex* vertices, uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount, const glm::mat4& transform)
{
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(Vertex) * vertexCount;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vData = {};
    vData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vb;
    m_device->CreateBuffer(&vbd, &vData, &vb);

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(uint32_t) * indexCount;
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iData = {};
    iData.pSysMem = indices;

    ComPtr<ID3D11Buffer> ib;
    m_device->CreateBuffer(&ibd, &iData, &ib);
 
    glm::mat4 mvp = m_projection * m_view * transform;

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    glm::mat4 mvpT = glm::transpose(mvp);
    memcpy(mapped.pData, &mvpT, sizeof(glm::mat4));
    m_context->Unmap(m_transformCB.Get(), 0);

    UINT stride = sizeof(Vertex), offset = 0;
    m_context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->DrawIndexed(indexCount, 0, 0);
}

void DX11Renderer::drawSubMesh(uint32_t indexOffset, uint32_t indexCount,
    const glm::mat4& transform)
{
    //-MaterialCB：テクスチャ無し
    MaterialCB mat;
    mat.useTexture = 0;
    D3D11_MAPPED_SUBRESOURCE matMapped;
    m_context->Map(m_materialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matMapped);
    memcpy(matMapped.pData, &mat, sizeof(MaterialCB));
    m_context->Unmap(m_materialCB.Get(), 0);

    //-mvp 更新
    glm::mat4 mvp = m_projection * m_view * transform;
    glm::mat4 mvpT = glm::transpose(mvp);

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &mvpT, sizeof(glm::mat4));
    m_context->Unmap(m_transformCB.Get(), 0);

    m_context->DrawIndexed(indexCount, indexOffset, 0);
}

void DX11Renderer::drawSubMeshTextured(uint32_t indexOffset, uint32_t indexCount, const glm::mat4& transform, ID3D11ShaderResourceView* srv)
{
    MaterialCB mat;
    mat.useTexture = 1;
    D3D11_MAPPED_SUBRESOURCE matMapped;
    m_context->Map(m_materialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matMapped);
    memcpy(matMapped.pData, &mat, sizeof(MaterialCB));
    m_context->Unmap(m_materialCB.Get(), 0);

    //-Bind Texture
    m_context->PSSetShaderResources(0, 1, &srv);

    //-Update MVP 
    glm::mat4 mvp = m_projection * m_view * transform;
    glm::mat4 mvpT = glm::transpose(mvp);
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD,0,&mapped);
    memcpy(mapped.pData, &mvpT, sizeof(glm::mat4));
    m_context->Unmap(m_transformCB.Get(),0);

    m_context->DrawIndexed(indexCount,indexOffset,0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

void DX11Renderer::beginOffscreen(uint32_t width, uint32_t height)
{
    if (!m_sceneRT)
        m_sceneRT = std::make_unique<RenderTexture>();

    m_sceneRT->resize(m_device.Get(), width, height);

    float clearColor[4] = {
        m_clearColor[0],m_clearColor[1],
        m_clearColor[2],m_clearColor[3] };
    m_sceneRT->clear(m_context.Get(), clearColor);
    m_sceneRT->bindAsRenderTarget(m_context.Get());

    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(),0);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(),nullptr,0);
    m_context->PSSetShader(m_pixelShader.Get(),nullptr,0);
    m_context->VSSetConstantBuffers(0,1,m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1,1,m_materialCB.GetAddressOf());
    m_context->PSSetSamplers(0,1,m_samplerState.GetAddressOf());

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_offscreen = true;
}

void DX11Renderer::endOffscreen()
{
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_offscreen = false;
}

} // namespace FaluEngine
