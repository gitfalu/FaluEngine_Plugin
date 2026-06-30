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
        PathResolver::resolveStr("assets/shaders/PBR.vert.hlsl"),
        PathResolver::resolveStr("assets/shaders/PBR.pixel.hlsl"))) 
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
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
        offsetof(Vertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,normal),D3D11_INPUT_PER_VERTEX_DATA},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BINORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,bitangent),D3D11_INPUT_PER_VERTEX_DATA,0},
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

    D3D11_BUFFER_DESC lbd = {};
    lbd.ByteWidth = sizeof(LightCB);
    lbd.Usage = D3D11_USAGE_DYNAMIC;
    lbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&lbd, nullptr, &m_lightCB);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateBuffer (LightCB) failed");
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

    //===== Shadow Depth Shader =======
    std::string shadowVSPath = PathResolver::resolveStr("assets/shaders/ShadowDepth.vert.hlsl");
    if (!std::filesystem::exists(shadowVSPath)) {
        LOG_ERROR("Shadow vertex shader nor found: {}", shadowVSPath);
        return false;
    }

    ComPtr<ID3DBlob> shadowVSBlob, shadowErrorBlob;
    hr = D3DCompileFromFile(
        std::wstring(shadowVSPath.begin(), shadowVSPath.end()).c_str(),
        nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
        &shadowVSBlob, &shadowErrorBlob
    );
    if (FAILED(hr)) {
        LOG_ERROR("Shadow VS error: {}", 
            static_cast<char*>(shadowErrorBlob->GetBufferPointer()));
        return false;
    }

    m_device->CreateVertexShader(
        shadowVSBlob->GetBufferPointer(),
        shadowVSBlob->GetBufferSize(),
        nullptr,&m_shadowVS
        );
    D3D11_INPUT_ELEMENT_DESC shadowLayout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
    };

    m_device->CreateInputLayout(
        shadowLayout, 1,
        shadowVSBlob->GetBufferPointer(),
        shadowVSBlob->GetBufferSize(),
        &m_shadowInputLayout
    );

    D3D11_BUFFER_DESC scbd = {};
    scbd.ByteWidth = sizeof(ShadowCB);
    scbd.Usage = D3D11_USAGE_DYNAMIC;
    scbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    scbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&scbd, nullptr, &m_shadowCB);

    D3D11_BUFFER_DESC sscbd = {};
    sscbd.ByteWidth = sizeof(ShadowSettingsCB);
    sscbd.Usage = D3D11_USAGE_DYNAMIC;
    sscbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sscbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&sscbd, nullptr, &m_shadowSettingsCB);

    m_dirShadowMap = std::make_unique<ShadowMap>();
    m_dirShadowMap->create(m_device.Get(), 2048);

    D3D11_RASTERIZER_DESC srd = {};
    srd.FillMode = D3D11_FILL_SOLID;
    srd.CullMode = D3D11_CULL_BACK;
    srd.FrontCounterClockwise = FALSE;
    srd.DepthBias = 1000;
    srd.DepthBiasClamp = 0.0f;
    srd.SlopeScaledDepthBias = 1.0f;
    srd.DepthClipEnable = TRUE;
    m_device->CreateRasterizerState(&srd, &m_shadowRasterizerState);

    LOG_INFO("Shadow shaders initialized");

    //==== SkySphere Shader ========
    {
        std::string skyVSPath = PathResolver::resolveStr("assets/shaders/SkySphere.vert.hlsl");
        std::string skyPSPath = PathResolver::resolveStr("assets/shaders/SkySphere.pixel.hlsl");

        ComPtr<ID3DBlob> skyVSBlob, skyPSBlob, skyErrBlob;

        hr = D3DCompileFromFile(
            std::wstring(skyVSPath.begin(), skyVSPath.end()).c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &skyVSBlob, &skyErrBlob);
        if (FAILED(hr))
        {
            if (skyErrBlob)
                LOG_ERROR("Sky VS error: {}",
                    static_cast<char*>(skyErrBlob->GetBufferPointer()));
            return false;
        }

        hr = D3DCompileFromFile(
            std::wstring(skyPSPath.begin(), skyPSPath.end()).c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &skyPSBlob, &skyErrBlob);
        if (FAILED(hr))
        {
            if (skyErrBlob)
                LOG_ERROR("Sky PS error: {}",
                    static_cast<char*>(skyErrBlob->GetBufferPointer()));
            return false;
        }

        m_device->CreateVertexShader(
            skyVSBlob->GetBufferPointer(), skyVSBlob->GetBufferSize(),
            nullptr, &m_skyVS
        );

        m_device->CreatePixelShader(
            skyPSBlob->GetBufferPointer(), skyPSBlob->GetBufferSize(),
            nullptr, &m_skyPS
        );

        // InputLayout
        D3D11_INPUT_ELEMENT_DESC skyLayout[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D11_INPUT_PER_VERTEX_DATA,0},
        };

        m_device->CreateInputLayout(
            skyLayout, 1,
            skyVSBlob->GetBufferPointer(), skyVSBlob->GetBufferSize(),
            &m_skyInputLayout);

        auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& buf)
            {
                D3D11_BUFFER_DESC d = {};
                d.ByteWidth = size;
                d.Usage = D3D11_USAGE_DYNAMIC;
                d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                m_device->CreateBuffer(&d, nullptr, &buf);
            };

        makeCB(sizeof(SkyCB), m_skyCB);
        makeCB(sizeof(SkySettingsCB), m_skySettingsCB);

        //=== 球のメッシュを生成 ========
        const int stacks = 32;
        const int slices = 32;
        const float radius = 1.0f;
        std::vector<glm::vec3> verts;
        std::vector<uint32_t> idxs;

        for (int i = 0; i <= stacks; ++i)
        {
            float phi = glm::pi<float>() * i / stacks;
            for (int j = 0; j <= slices; ++j)
            {
                float theta = 2.0f * glm::pi<float>() * j / slices;
                glm::vec3 v;
                v.x = radius * sinf(phi) * cosf(theta);
                v.y = radius * cosf(phi);
                v.z = radius * sinf(phi) * sinf(theta);
                verts.push_back(v);
            }
        }
        for (int i = 0; i < stacks; ++i)
        {
            for (int j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                idxs.push_back(a); idxs.push_back(b); idxs.push_back(a + 1);
                idxs.push_back(b); idxs.push_back(b + 1); idxs.push_back(a + 1);
            }
        }
        m_skyIndexCount = static_cast<uint32_t>(idxs.size());

        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth = static_cast<UINT>(sizeof(glm::vec3) * verts.size());
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vd = {};
        vd.pSysMem = verts.data();
        m_device->CreateBuffer(&vbd, &vd, &m_skyVB);

        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * idxs.size());
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA id = {};
        id.pSysMem = idxs.data();
        m_device->CreateBuffer(&ibd, &id, &m_skyIB);

        LOG_INFO("SkySphere shaders initialized");
    }

    // EquirectToCubemap
    {
        std::string cubeVSPath = PathResolver::resolveStr("assets/shaders/EquirectToCubemap.vert.hlsl");
        std::string cubePSPath = PathResolver::resolveStr("assets/shaders/EquirectToCubemap.pixel.hlsl");

        ComPtr<ID3DBlob> cubeVSBlob, cubePSBlob, cubeErrBlob;

        hr = D3DCompileFromFile(
            std::wstring(cubeVSPath.begin(), cubeVSPath.end()).c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &cubeVSBlob, &cubeErrBlob);
        if (FAILED(hr))
        {
            if (cubeErrBlob)
                LOG_ERROR("Cubemap VS error: {}", static_cast<char*>(cubeErrBlob->GetBufferPointer()));
            return false;
        }

        hr = D3DCompileFromFile(
            std::wstring(cubePSPath.begin(), cubePSPath.end()).c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &cubePSBlob, &cubeErrBlob);
        if (FAILED(hr))
        {
            if (cubeErrBlob)
                LOG_ERROR("Cubemap PS error: {}", static_cast<char*>(cubeErrBlob->GetBufferPointer()));
            return false;
        }

        m_device->CreateVertexShader(
            cubeVSBlob->GetBufferPointer(), cubeVSBlob->GetBufferSize(),
            nullptr, &m_cubemapVS);
        m_device->CreatePixelShader(
            cubePSBlob->GetBufferPointer(), cubePSBlob->GetBufferSize(),
            nullptr, &m_cubemapPS);

        D3D11_INPUT_ELEMENT_DESC cubeLayout[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D11_INPUT_PER_VERTEX_DATA,0},
        };
        m_device->CreateInputLayout(
            cubeLayout, 1,
            cubeVSBlob->GetBufferPointer(), cubeVSBlob->GetBufferSize(),
            &m_cubemapInputLayout);

        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth = sizeof(SkyCB);
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&cbd, nullptr, &m_cubemapCB);

        m_environmentMap = std::make_unique<EnvironmentMap>();
        m_environmentMap->create(m_device.Get(), 512);

        LOG_INFO("EquirectToCubemap shaders initialized");
    }

    // IrradianceConvolution
    {
        std::string irrPSPath = PathResolver::resolveStr("assets/shaders/IrradianceConvolution.pixel.hlsl");

        ComPtr<ID3DBlob> irrPSBlob, irrErrBlob;
        hr = D3DCompileFromFile(
            std::wstring(irrPSPath.begin(), irrPSPath.end()).c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &irrPSBlob, &irrErrBlob);
        if (FAILED(hr))
        {
            if (irrErrBlob)
                LOG_ERROR("Irradiance PS error: {}", static_cast<char*>(irrErrBlob->GetBufferPointer()));
            return false;
        }

        m_device->CreatePixelShader(
            irrPSBlob->GetBufferPointer(), irrPSBlob->GetBufferSize(),
            nullptr, &m_irradiancePS);

        m_irradianceMap = std::make_unique<EnvironmentMap>();
        m_irradianceMap->create(m_device.Get(), 32);

        LOG_INFO("IrradianceConvolution shader initialized");
    }

    // PrefilterEnvironment
    {
        std::string prefPSPath = PathResolver::resolveStr("assets/shaders/PrefilterEnvironment.pixel.hlsl");

        ComPtr<ID3DBlob> prefPSBlob, prefErrBlob;
        hr = D3DCompileFromFile(
            std::wstring(prefPSPath.begin(), prefPSPath.end()).c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &prefPSBlob, &prefErrBlob);
        if (FAILED(hr))
        {
            if (prefErrBlob)
                LOG_ERROR("Prefilter PS error: {}", static_cast<char*>(prefErrBlob->GetBufferPointer()));
            return false;
        }

        m_device->CreatePixelShader(
            prefPSBlob->GetBufferPointer(), prefPSBlob->GetBufferSize(),
            nullptr, &m_prefilterPS);

        D3D11_BUFFER_DESC pcbd = {};
        pcbd.ByteWidth = sizeof(PrefilterCB);
        pcbd.Usage = D3D11_USAGE_DYNAMIC;
        pcbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        pcbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&pcbd, nullptr, &m_prefilterCB);

        m_prefilterMap = std::make_unique<EnvironmentMap>();
        m_prefilterMap->create(m_device.Get(), 128, 5);

        LOG_INFO("PrefilterEnvitornment shader initialized");
    }

    // BRDF LUT
    {
        std::string lutVSPath = PathResolver::resolveStr("assets/shaders/BRDFLUT.vert.hlsl");
        std::string lutPSPath = PathResolver::resolveStr("assets/shaders/BRDFLUT.pixel.hlsl");

        ComPtr<ID3DBlob> lutVSBlob, lutPSBlob, lutErrBlob;

        hr = D3DCompileFromFile(
            std::wstring(lutVSPath.begin(), lutVSPath.end()).c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &lutVSBlob, &lutErrBlob);
        if (FAILED(hr))
        {
            if (lutErrBlob)
                LOG_ERROR("BRDFLUT VS error: {}", static_cast<char*>(lutErrBlob->GetBufferPointer()));
            return false;
        }

        hr = D3DCompileFromFile(
            std::wstring(lutPSPath.begin(), lutPSPath.end()).c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &lutPSBlob, &lutErrBlob);
        if (FAILED(hr))
        {
            if (lutErrBlob)
                LOG_ERROR("BRDFLUT VS error: {}", static_cast<char*>(lutErrBlob->GetBufferPointer()));
            return false;
        }

        m_device->CreateVertexShader(
            lutVSBlob->GetBufferPointer(), lutVSBlob->GetBufferSize(),
            nullptr, &m_brdfLutVS);
        m_device->CreateVertexShader(
            lutVSBlob->GetBufferPointer(), lutVSBlob->GetBufferSize(),
            nullptr, &m_brdfLutVS);

        D3D11_TEXTURE2D_DESC lutDesc = {};
        lutDesc.Width = 512;
        lutDesc.Height = 512;
        lutDesc.MipLevels = 1;
        lutDesc.ArraySize = 1;
        lutDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
        lutDesc.SampleDesc.Count = 1;
        lutDesc.Usage = D3D11_USAGE_DEFAULT;
        lutDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        m_device->CreateTexture2D(&lutDesc, nullptr, &m_brdfLutTexture);
        m_device->CreateRenderTargetView(m_brdfLutTexture.Get(), nullptr, &m_brdfLutRTV);
        m_device->CreateShaderResourceView(m_brdfLutTexture.Get(), nullptr, &m_brdfLutSRV);

        LOG_INFO("BRDFLUT shaders initialized");
    }

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

    D3D11_SAMPLER_DESC shadowSD = {};
    shadowSD.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    shadowSD.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSD.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSD.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSD.BorderColor[0] = 1.0f;
    shadowSD.BorderColor[1] = 1.0f;
    shadowSD.BorderColor[2] = 1.0f;
    shadowSD.BorderColor[3] = 1.0f;
    shadowSD.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSD.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> shadowSampler;
    m_device->CreateSamplerState(&shadowSD, &shadowSampler);
    m_context->PSSetSamplers(1, 1, shadowSampler.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC skyDSD = {};
    skyDSD.DepthEnable = TRUE;
    skyDSD.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyDSD.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_device->CreateDepthStencilState(&skyDSD, &m_skyDepthState);

    // BRDF lut
    D3D11_SAMPLER_DESC lutSD = {};
    lutSD.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lutSD.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    lutSD.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    lutSD.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    lutSD.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    lutSD.MaxLOD = D3D11_FLOAT32_MAX;
    m_device->CreateSamplerState(&lutSD, &m_lutSampler);

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

void DX11Renderer::updateLights(const LightCB& lightData)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_lightCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &lightData, sizeof(LightCB));
    m_context->Unmap(m_lightCB.Get(), 0);
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
    m_context->PSSetConstantBuffers(2, 1, m_lightCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    if (m_dirShadowMap && m_dirShadowMap->isValid())
        m_dirShadowMap->bindForRead(m_context.Get(), 5);

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;
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

void DX11Renderer::drawSubMeshPBR(uint32_t indexOffset, uint32_t indexCount,
    const glm::mat4& transform,
    class MaterialAsset* material)
{
    bool hasCustom = material && material->cachedShader &&
        material->cachedShader->valid;

    ID3D11VertexShader* vs = hasCustom
        ? material->cachedShader->vertexShader.Get() : m_vertexShader.Get();
    ID3D11PixelShader* ps = hasCustom
        ? material->cachedShader->pixelShader.Get() : m_pixelShader.Get();
    ID3D11InputLayout* layout = hasCustom
        ? material->cachedShader->inputLayout.Get() : m_inputLayout.Get();

    if (m_boundVS != vs) { m_context->VSSetShader(vs, nullptr, 0); m_boundVS = vs; }
    if (m_boundPS != ps) { m_context->PSSetShader(ps, nullptr, 0); m_boundPS = ps; }
    m_context->IASetInputLayout(layout);

    //==== Material Parameter =========
    MaterialCB mat;
    if (material)
    {
        mat.albedoColor = material->albedoColor;
        mat.metallic = material->metallic;
        mat.roughness = material->roughness;
        mat.useAlbedoMap = (material->cachedAlbedoMap && material->cachedAlbedoMap->srv) ? 1 : 0;
        mat.useMetallicMap = (material->cachedMetallicMap && material->cachedMetallicMap->srv) ? 1 : 0;
        mat.useNormalMap = (material->cachedNormalMap && material->cachedNormalMap->srv) ? 1 : 0;
        mat.useAOMap = (material->cachedAOMap && material->cachedAOMap->srv) ? 1 : 0;
        mat.useEmissiveMap = (material->cachedEmissiveMap && material->cachedEmissiveMap->srv) ? 1 : 0;
        mat.emissiveStrength = material->emissiveStrength;
        mat.emissiveColor = material->emissiveColor;
    }

    D3D11_MAPPED_SUBRESOURCE matMapped;
    m_context->Map(m_materialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matMapped);
    memcpy(matMapped.pData, &mat, sizeof(MaterialCB));
    m_context->Unmap(m_materialCB.Get(), 0);

    //===== TransformCB =======
    TransformCB cb;
    cb.mvp = glm::transpose(m_projection * m_view * transform);
    cb.world = glm::transpose(transform);
    cb.normalMatrix = glm::inverse(transform);

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cb, sizeof(TransformCB));
    m_context->Unmap(m_transformCB.Get(), 0);

    //===== Texture Bind =======
    ID3D11ShaderResourceView* srvs[5] = { nullptr,nullptr,nullptr,nullptr,nullptr };
    if (material)
    {
        if (mat.useAlbedoMap) srvs[0] = material->cachedAlbedoMap->srv.Get();
        if (mat.useMetallicMap) srvs[1] = material->cachedMetallicMap->srv.Get();
        if (mat.useNormalMap) srvs[2] = material->cachedNormalMap->srv.Get();
        if (mat.useAOMap) srvs[3] = material->cachedAOMap->srv.Get();
        if (mat.useEmissiveMap) srvs[4] = material->cachedEmissiveMap->srv.Get();
    }
    m_context->PSSetShaderResources(0, 5, srvs);

    // IBL Texture
    ID3D11ShaderResourceView* iblSRVs[3] = {
        m_irradianceMap ? m_irradianceMap->getSRV() : nullptr,
        m_prefilterMap ? m_prefilterMap->getSRV() : nullptr,
        m_brdfLutSRV.Get()
    };

    m_context->PSSetShaderResources(6, 3, iblSRVs);
    m_context->PSSetSamplers(2, 1, m_lutSampler.GetAddressOf());

    m_context->DrawIndexed(indexCount, indexOffset, 0);

    ID3D11ShaderResourceView* nullSRVs[8] = { 
        nullptr,nullptr,nullptr,nullptr,nullptr,
        nullptr,nullptr,nullptr};
    m_context->PSSetShaderResources(0, 8, nullSRVs);

}

void DX11Renderer::drawSkySphere(const glm::mat4& view, const glm::mat4& proj, const SkySettingsCB& settings, ID3D11ShaderResourceView* srv)
{
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));

    SkyCB skyCB;
    skyCB.viewProj = glm::transpose(proj * viewNoTrans);

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_skyCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &skyCB, sizeof(SkyCB));
    m_context->Unmap(m_skyCB.Get(), 0);

    m_context->Map(m_skySettingsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &settings, sizeof(SkySettingsCB));
    m_context->Unmap(m_skySettingsCB.Get(), 0);

    m_context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);

    m_context->RSSetState(nullptr);

    m_context->IASetInputLayout(m_skyInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_skyVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_skyPS.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_skyCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_skySettingsCB.GetAddressOf());

    if (srv) m_context->PSSetShaderResources(0, 1, &srv);

    UINT stride = sizeof(glm::vec3), offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_skyIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->DrawIndexed(m_skyIndexCount, 0, 0);

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());

    if (srv)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
    }

    m_boundVB = nullptr;
    m_boundIB = nullptr;
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
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_lightCB.GetAddressOf());
    m_context->PSSetSamplers(0,1,m_samplerState.GetAddressOf());
    if (m_dirShadowMap && m_dirShadowMap->isValid())
        m_dirShadowMap->bindForRead(m_context.Get(), 5);

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;
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

// Game View

void DX11Renderer::beginGameOffscreen(uint32_t width, uint32_t height)
{
    if (!m_gameRT)
        m_gameRT = std::make_unique<RenderTexture>();

    m_gameRT->resize(m_device.Get(), width, height);

    float clearColor[4] = { m_clearColor[0],m_clearColor[1],
                            m_clearColor[2],m_clearColor[3] };

    m_gameRT->clear(m_context.Get(), clearColor);
    m_gameRT->bindAsRenderTarget(m_context.Get());

    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_lightCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    if (m_dirShadowMap && m_dirShadowMap->isValid())
        m_dirShadowMap->bindForRead(m_context.Get(), 5);

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;
    m_gameOffscreen = true;
}

void DX11Renderer::endGameOffscreen()
{
    m_gameOffscreen = false;

    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;
}
//=========== 

void DX11Renderer::beginShadowPass(const glm::mat4& lightView, const glm::mat4& lightProj)
{
    m_dirShadowMap->lightView = lightView;
    m_dirShadowMap->lightProjection = lightProj;
    m_dirShadowMap->clear(m_context.Get());
    m_dirShadowMap->bindForWrite(m_context.Get());

    m_context->RSSetState(m_shadowRasterizerState.Get());
    m_context->IASetInputLayout(m_shadowInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_shadowVS.Get(), nullptr, 0);
    m_context->PSSetShader(nullptr, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_shadowCB.GetAddressOf());

    m_boundVB = nullptr;
    m_boundIB = nullptr;
}

void DX11Renderer::endShadowPass()
{
    if (m_offscreen && m_sceneRT && m_sceneRT->isValid())
    {
        m_sceneRT->bindAsRenderTarget(m_context.Get());
    }
    else if (m_gameOffscreen && m_gameRT && m_gameRT->isValid())
    {
        m_gameRT->bindAsRenderTarget(m_context.Get());
    }
    else
    {
        m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
        updateViewport();
    }

    // シェーダーを通常描画用に戻す ← 追加
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_lightCB.GetAddressOf());
    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    m_dirShadowMap->bindForRead(m_context.Get(), 5);

    m_boundVB = nullptr;
    m_boundIB = nullptr;
}

void DX11Renderer::drawShadowMesh(uint32_t indexOffset, uint32_t indexCount, const glm::mat4& world)
{
    ShadowCB cb;
    cb.lightMVP = glm::transpose(m_dirShadowMap->lightProjection *
        m_dirShadowMap->lightView * world);
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_shadowCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cb, sizeof(ShadowCB));
    m_context->Unmap(m_shadowCB.Get(), 0);

    m_context->DrawIndexed(indexCount, indexOffset,0);
}

void DX11Renderer::updateShadowSettings(const ShadowSettingsCB& settings)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_shadowSettingsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &settings, sizeof(ShadowSettingsCB));
    m_context->Unmap(m_shadowSettingsCB.Get(), 0);

    m_context->PSSetConstantBuffers(3, 1, m_shadowSettingsCB.GetAddressOf());
}

void DX11Renderer::generateEnvironmentMap(const SkySettingsCB& settings, ID3D11ShaderResourceView* skySRV)
{
    if (!m_environmentMap) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_skySettingsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &settings, sizeof(SkySettingsCB));
    m_context->Unmap(m_skySettingsCB.Get(), 0);

    m_context->IASetInputLayout(m_cubemapInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_cubemapVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_cubemapPS.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_cubemapCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_skySettingsCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    if (skySRV) m_context->PSSetShaderResources(0, 1, &skySRV);

    UINT stride = sizeof(glm::vec3), offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_skyIB.Get(), DXGI_FORMAT_R32_UINT, 0);

    glm::mat4 proj = EnvironmentMap::getCubeProjectionMatrix();
    uint32_t size = m_environmentMap->getSize();

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(size);
    vp.Height = static_cast<float>(size);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);
    m_context->RSSetState(nullptr);

    for(int face = 0 ; face < 6;++face)
    {
        ID3D11RenderTargetView* rtv = m_environmentMap->getFaceRTV(face);
        m_context->OMSetRenderTargets(1, &rtv, nullptr);

        glm::mat4 view = EnvironmentMap::getFaceViewMatrix(face);
        SkyCB skyCB;
        skyCB.viewProj = glm::transpose(proj * view);

        D3D11_MAPPED_SUBRESOURCE cbMapped;
        m_context->Map(m_cubemapCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
        memcpy(cbMapped.pData, &skyCB, sizeof(SkyCB));
        m_context->Unmap(m_cubemapCB.Get(), 0);

        m_context->DrawIndexed(m_skyIndexCount, 0, 0);
    }

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    updateViewport();

    if (skySRV)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
    }

    LOG_INFO("EnvironmentMap generated from SkySphere");
}

void DX11Renderer::generateIrradianceMap()
{
    if (!m_irradianceMap || !m_environmentMap) return;
    auto* envSRV = m_environmentMap->getSRV();
    if (!envSRV) return;

    m_context->IASetInputLayout(m_cubemapInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_cubemapVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_irradiancePS.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_cubemapCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, &envSRV);

    UINT stride = sizeof(glm::vec3), offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_skyIB.Get(), DXGI_FORMAT_R32_UINT, 0);

    glm::mat4 proj = EnvironmentMap::getCubeProjectionMatrix();
    uint32_t size = m_irradianceMap->getSize();

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(size);
    vp.Height = static_cast<float>(size);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);
    m_context->RSSetState(nullptr);

    for (int face = 0; face < 6; ++face)
    {
        ID3D11RenderTargetView* rtv = m_irradianceMap->getFaceRTV(face);
        m_context->OMSetRenderTargets(1, &rtv, nullptr);

        glm::mat4 view = EnvironmentMap::getFaceViewMatrix(face);
        SkyCB skyCB;
        skyCB.viewProj = glm::transpose(proj * view);

        D3D11_MAPPED_SUBRESOURCE cbMapped;
        m_context->Map(m_cubemapCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
        memcpy(cbMapped.pData, &skyCB, sizeof(SkyCB));
        m_context->Unmap(m_cubemapCB.Get(), 0);

        m_context->DrawIndexed(m_skyIndexCount, 0, 0);
    }

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    updateViewport();

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);

    LOG_INFO("IrradianceMap generated ({}x{} x6)", size, size);
}

void DX11Renderer::generatePrefilterMap()
{
    if (!m_prefilterMap || !m_environmentMap) return;

    auto* envSRV = m_environmentMap->getSRV();
    if (!envSRV) return;

    m_context->IASetInputLayout(m_cubemapInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_cubemapVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_prefilterPS.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_cubemapCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_prefilterCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, &envSRV);

    UINT stride = sizeof(glm::vec3), offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyCB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_skyIB.Get(), DXGI_FORMAT_R32_UINT, 0);

    glm::mat4 proj = EnvironmentMap::getCubeProjectionMatrix();

    m_context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);
    m_context->RSSetState(nullptr);

    uint32_t maxMips = m_prefilterMap->getMipLevels();
    for (uint32_t mip = 0; mip < maxMips; ++mip)
    {
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMips - 1);

        PrefilterCB prefCB;
        prefCB.roughness = roughness;
        D3D11_MAPPED_SUBRESOURCE prefMapped;
        m_context->Map(m_prefilterCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &prefMapped);
        memcpy(prefMapped.pData, &prefCB, sizeof(PrefilterCB));
        m_context->Unmap(m_prefilterCB.Get(), 0);

        uint32_t mipSize = m_prefilterMap->getSize() >> mip;
        if (mipSize < 1)mipSize = 1;

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(mipSize);
        vp.Height = static_cast<float>(mipSize);
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        for (int face = 0; face < 6; ++face)
        {
            ID3D11RenderTargetView* rtv = m_prefilterMap->getFaceRTV(face, mip);
            m_context->OMSetRenderTargets(1, &rtv, nullptr);

            glm::mat4 view = EnvironmentMap::getFaceViewMatrix(face);
            SkyCB skyCB;
            skyCB.viewProj = glm::transpose(proj * view);

            D3D11_MAPPED_SUBRESOURCE cbMapped;
            m_context->Map(m_cubemapCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
            memcpy(cbMapped.pData, &skyCB, sizeof(SkyCB));
            m_context->Unmap(m_cubemapCB.Get(), 0);

            m_context->DrawIndexed(m_skyIndexCount, 0, 0);
        }
    }

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    updateViewport();

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);

    LOG_INFO("PrefilterMap generated ({} mips)", maxMips);
}

void DX11Renderer::generateBRDFLUT()
{
    if (!m_brdfLutRTV) return;

    ID3D11RenderTargetView* rtv = m_brdfLutRTV.Get();
    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = 512.0f;
    vp.Height = 512.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);
    m_context->RSSetState(nullptr);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_brdfLutVS.Get(), nullptr,0);
    m_context->PSSetShader(m_brdfLutPS.Get(), nullptr, 0);

    ID3D11Buffer* nullVB = nullptr;
    UINT stride = 0, offset = 0;
    m_context->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);

    m_context->Draw(3, 0);

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    updateViewport();

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;

    LOG_INFO("BRDFLUT generated (512x512)");
}

} // namespace FaluEngine
