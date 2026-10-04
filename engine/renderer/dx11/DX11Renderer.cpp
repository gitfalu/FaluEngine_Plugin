/*****************************************************************//**
 * \file   DX11Renderer.cpp
 * \brief  DirectXでのレンダリングをサポートするAPI
 * 
 * \author Falu
 * \date   October 2026
 *********************************************************************/

#include "DX11Renderer.h"
#include "core/Logger.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Component.h"
#include "core/PathResolver.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace FaluEngine {
/// @brief DrectXでのWindow描画
/// @param windowHandle ハンドル
/// @param width 横幅
/// @param height 縦幅
/// @return 初期化成功/失敗
bool DX11Renderer::init(void* windowHandle, uint32_t width, uint32_t height) {
    m_width  = width;
    m_height = height;
    HWND hwnd = static_cast<HWND>(windowHandle);

    //-SwapChainの作成
    if (!createDeviceAndSwapChain(hwnd))       return false;
    //-レンダーターゲットを作成
    if (!createRenderTargetView())    return false;
    LOG_INFO("[Renderer] Created RenderTargetView");
    //-深度ステンシルビューを作成、深度バッファを有効にする
    if (!createDepthStencilView())    return false;
    LOG_INFO("[Renderer] Created DSV");
    //-シェーダーを作成(ベースはPBR)
    if (!createShaders(
        PathResolver::resolve("assets/shaders/PBR.vert.hlsl"),
        PathResolver::resolve("assets/shaders/PBR.pixel.hlsl"))) 
        return false;
    //-通常状態を初期化
    if (!createDefaultStates()) return false;

    //-初回のみViewPortを一回更新
    updateViewport();

    //-アスペクト比を計算し投影に変換
    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    m_projection = glm::perspectiveLH(glm::radians(60.0f), aspect, 1.0f, 1000.0f);

    LOG_INFO("DX11Renderer initialized ({}x{})", m_width, m_height);
    return true;
}

namespace
{
    /// @brief 文字を小文字に変換するヘルパー関数
    /// @param s 変換前の文字列
    /// @return 
    std::string toLowerAscii(std::string s)
    {
        for (auto& c : s)
        {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return s;
    }

    /// @brief 使用するGPUを文字列から選ぶ。優先準備：環境変数>NVIDIA>専用VRAMが最大のもの   
    /// @param factory 全てのGPU
    /// @param outName 出力名
    /// @return 指定したGPU/発見されなかった場合デフォルトGPU
    ComPtr<IDXGIAdapter1> pickAdapter(IDXGIFactory1* factory, std::string& outName)
    {
        struct Candidate
        {
            ComPtr<IDXGIAdapter1> adapter;
            DXGI_ADAPTER_DESC1 desc = {}; // アダプターについての詳細がまとめられたもの
            std::string name;
        };
        std::vector<Candidate> list;// 見つかった全てのGPUを登録する

        // 登録されているGPUデバイスを捜査する
        for (UINT i = 0;; ++i)
        {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
            // 構造体に見つかったアダプターを登録
            Candidate c;
            adapter->GetDesc1(&c.desc);
            c.adapter = adapter;
            c.name = PathResolver::toUtf8(std::filesystem::path(c.desc.Description));
            //-
            const bool software = (c.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            LOG_INFO("[Renderer] Adapter[{}]: {} (VRAM {} MB{})",
                i, c.name, static_cast<uint64_t>(c.desc.DedicatedVideoMemory / (1024 * 1024)),
                software ? ",software" : "");
            if (software) continue;

            list.push_back(std::move(c));
        }
        //-見つからなかった場合早期にnullptrを返す
        if (list.empty()) return nullptr;

        //-VRAMが少ない方を選出するためのラムダ式
        auto lessVram = [](const Candidate& a, const Candidate& b)
            {
                return a.desc.DedicatedVideoMemory < b.desc.DedicatedVideoMemory;
            };
        const Candidate* chosen = nullptr;

        //-環境変数にあったGPUを選出
        char env[128] = {};
        const DWORD envLen = GetEnvironmentVariableA("FALU_GPU", env, static_cast<DWORD>(sizeof(env)));
        if (envLen > 0 && envLen < sizeof(env))
        {
            const std::string want = toLowerAscii(env);
            for (const auto& c : list)
            {
                if (toLowerAscii(c.name).find(want) != std::string::npos &&
                    (!chosen || lessVram(*chosen, c)))
                    chosen = &c;
            }
            if (!chosen) LOG_WARN("[Rendereer] FALU_GPU='{}' matched no adapter", env);
        }
        //-NVIDIAのGPU且つVRAMの大きいアダプターを選出
        if (!chosen)
        {
            for (const auto& c : list)
            {
                if (c.desc.VendorId == 0x10DE && (!chosen || lessVram(*chosen, c)))
                {
                    chosen = &c;
                }
            }
        }
        //-指定なしでVRAMの大きいアダプターを選出
        if (!chosen)
        {
            chosen = &*std::max_element(list.begin(), list.end(), lessVram);
        }

        //-走査して見つかったGPUの名前を出力する名前に登録しアダプターを返す
        outName = chosen->name;
        return chosen->adapter;
    }
}// namespace

/// @brief スワップチェインの作成
/// @param hwnd WindowHandle
/// @return 初期化成功か否か
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

    //-GPUを選出してから作成する
    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    {
        std::string adapterName;
        ComPtr<IDXGIAdapter1> adapter = pickAdapter(factory.Get(), adapterName);
        if (adapter)
        {
            HRESULT hr = D3D11CreateDevice(
                adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                &m_device, &featureLevel, &m_context
            );
            
            if (SUCCEEDED(hr))
                hr = factory->CreateSwapChain(m_device.Get(), &sd, &m_swapChain);

            if (SUCCEEDED(hr))
            {
                m_adapterName = adapterName;
            }
            else
            {
                LOG_WARN("[Renderer] Deivce creation on '{}' failed: ox{:08X}. Falling back to default adapter.",
                    adapterName, static_cast<uint32_t>(hr));
                m_swapChain.Reset();
                m_context.Reset();
                m_device.Reset();
            }
        }
    }

    if (!m_swapChain)
    {
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
            &sd, &m_swapChain, &m_device, &featureLevel, &m_context
        );

        if (FAILED(hr))
        {
            LOG_ERROR("D3D11CreateDeviceAndSwapChain failed: ox{:08X}", static_cast<uint32_t>(hr));
            return false;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> dxgiAdapter;
        //-デバイスとアダプターが取得できた場合、アダプターの名称を保管する
        if (SUCCEEDED(m_device.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter)))
        {
            DXGI_ADAPTER_DESC d = {};
            dxgiAdapter->GetDesc(&d);
            m_adapterName = PathResolver::toUtf8(std::filesystem::path(d.Description));
        }
    }

    LOG_INFO("[Renderer] Created SwapChain (GPU: {})", m_adapterName);
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

bool DX11Renderer::createShaders(const std::filesystem::path& vsPath, const std::filesystem::path& psPath)
{
    //-デフォルト頂点シェーダーファイルの存在確認
    if (!std::filesystem::exists(vsPath)) {
        LOG_ERROR("Vertex shader not found: '{}'", vsPath.string());
        return false;
    }

    //-シェーダーのコンパイルとコンパイルの成功/不成功に使用する変数の宣言
    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    UINT compileFlags = 0;
#ifdef ENGINE_DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    //-デフォルト頂点シェーダーコンパイル
    HRESULT hr = D3DCompileFromFile(
        vsPath.wstring().c_str(),
        nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
        &vsBlob,&errorBlob
    );

    if (FAILED(hr)) {
        if(errorBlob)
            LOG_ERROR("VS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    //-デフォルトピクセルシェーダーの存在確認
    if (!std::filesystem::exists(psPath)) {
        LOG_ERROR("Pixel shader not found: {}", psPath.string());
        return false;
    }
    //-デフォルトピクセルシェーダーのコンパイル
    hr = D3DCompileFromFile(
        psPath.wstring().c_str(),
        nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
        &psBlob, &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob)
            LOG_ERROR("PS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    //-シェーダーを作成
    m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

    //-頂点レイアウトの作成
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
    //-入力値データを登録
    hr = m_device->CreateInputLayout(
        layout, ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_inputLayout
    );

    if (FAILED(hr)) {
        LOG_ERROR("CreateInputLayout failed");
        return false;
    }
    //-トランスフォームバッファを作成
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
    //-ライトバッファを作成
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
    //-マテリアルバッファを作成
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
    //-サンプラーを作成
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
    //-パスを変換しファイルの存在確認
    std::filesystem::path shadowVSPath = PathResolver::resolve("assets/shaders/ShadowDepth.vert.hlsl");
    if (!std::filesystem::exists(shadowVSPath)) {
        LOG_ERROR("Shadow vertex shader nor found: {}", shadowVSPath.string());
        return false;
    }

    ComPtr<ID3DBlob> shadowVSBlob, shadowErrorBlob;
    //-シェーダーのコンパイル
    hr = D3DCompileFromFile(
        shadowVSPath.wstring().c_str(),
        nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
        &shadowVSBlob, &shadowErrorBlob
    );
    if (FAILED(hr)) {
        LOG_ERROR("Shadow VS error: {}", 
            static_cast<char*>(shadowErrorBlob->GetBufferPointer()));
        return false;
    }
    //-シェーダーを作成
    m_device->CreateVertexShader(
        shadowVSBlob->GetBufferPointer(),
        shadowVSBlob->GetBufferSize(),
        nullptr,&m_shadowVS
        );
    //-入力データを作成
    D3D11_INPUT_ELEMENT_DESC shadowLayout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    //-入力データを登録
    m_device->CreateInputLayout(
        shadowLayout, 1,
        shadowVSBlob->GetBufferPointer(),
        shadowVSBlob->GetBufferSize(),
        &m_shadowInputLayout
    );
    //-影バッファを作成
    D3D11_BUFFER_DESC scbd = {};
    scbd.ByteWidth = sizeof(ShadowCB);
    scbd.Usage = D3D11_USAGE_DYNAMIC;
    scbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    scbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&scbd, nullptr, &m_shadowCB);
    //-影設定バッファを作成
    D3D11_BUFFER_DESC sscbd = {};
    sscbd.ByteWidth = sizeof(ShadowSettingsCB);
    sscbd.Usage = D3D11_USAGE_DYNAMIC;
    sscbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sscbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&sscbd, nullptr, &m_shadowSettingsCB);

    // DefaultのShadowSettingsを設定
    ShadowSettingsCB defaultSettings;
    defaultSettings.useShadow = 0;
    updateShadowSettings(defaultSettings);

    m_dirShadowMap = std::make_unique<ShadowMap>();
    m_dirShadowMap->create(m_device.Get(), 2048);
    //-ラスタライザを作成
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

    //===== Skinned Shadow Dpeth Shdaer ====
    //-スキンメッシュのシェーダーパスを変換し存在確認
    std::filesystem::path shadowSkinnedVSPath = PathResolver::resolve("assets/shaders/ShadowDepth_Skinned.vert.hlsl");
    if (!std::filesystem::exists(shadowSkinnedVSPath))
    {
        LOG_ERROR("Shadow(Skinned) vertex shader not found: {}", shadowSkinnedVSPath.string());
        return false;
    }

    ComPtr<ID3DBlob> shadowSkinnedVSBlob, shadowSkinnedErrBlob;
    //-スキンメッシュシェーダーをコンパイル
    hr = D3DCompileFromFile(
        shadowSkinnedVSPath.wstring().c_str(),
        nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
        &shadowSkinnedVSBlob, &shadowSkinnedErrBlob
    );
    if (FAILED(hr))
    {
        LOG_ERROR("Shadow(Skinned) VS error: {}",
            static_cast<char*>(shadowSkinnedErrBlob->GetBufferPointer()));
        return false;
    }
    //-頂点シェーダーを作成
    m_device->CreateVertexShader(
        shadowSkinnedVSBlob->GetBufferPointer(),
        shadowSkinnedVSBlob->GetBufferSize(),
        nullptr, &m_shadowSkinnedVS
    );
    //-スキンメッシュ用の影の入力値作成
    D3D11_INPUT_ELEMENT_DESC shadowSkinnedLayout[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(SkinnedVertex,position),
        D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEINDICES",0,DXGI_FORMAT_R32G32B32A32_SINT,0,offsetof(SkinnedVertex,boneIndices),
        D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEWEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(SkinnedVertex,boneWeights),
        D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    //-スキンメッシュ用の影の入力値の登録
    m_device->CreateInputLayout(
        shadowSkinnedLayout, 3,
        shadowSkinnedVSBlob->GetBufferPointer(),
        shadowSkinnedVSBlob->GetBufferSize(),
        &m_shadowSkinnedInputLayout
    );

    //==== SkySphere Shader ========
    {
        //-空用のシェーダーパスを変換
        std::filesystem::path skyVSPath = PathResolver::resolve("assets/shaders/SkySphere.vert.hlsl");
        std::filesystem::path skyPSPath = PathResolver::resolve("assets/shaders/SkySphere.pixel.hlsl");

        if (!std::filesystem::exists(skyVSPath))
        {
            LOG_ERROR("SkySphere vertex shader not found: {}", skyVSPath.string());
            return false;
        }

        if (!std::filesystem::exists(skyPSPath))
        {
            LOG_ERROR("SkySphere pixel shader not found: {}", skyPSPath.string());
            return false;
        }

        ComPtr<ID3DBlob> skyVSBlob, skyPSBlob, skyErrBlob;
        //-空用頂点シェーダーのコンパイル
        hr = D3DCompileFromFile(
            skyVSPath.wstring().c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &skyVSBlob, &skyErrBlob);
        if (FAILED(hr))
        {
            if (skyErrBlob)
                LOG_ERROR("Sky VS error: {}",
                    static_cast<char*>(skyErrBlob->GetBufferPointer()));
            return false;
        }
        //-空用ピクセルシェーダーのコンパイル
        hr = D3DCompileFromFile(
            skyPSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &skyPSBlob, &skyErrBlob);
        if (FAILED(hr))
        {
            if (skyErrBlob)
                LOG_ERROR("Sky PS error: {}",
                    static_cast<char*>(skyErrBlob->GetBufferPointer()));
            return false;
        }
        //-シェーダーの登録
        m_device->CreateVertexShader(
            skyVSBlob->GetBufferPointer(), skyVSBlob->GetBufferSize(),
            nullptr, &m_skyVS
        );

        m_device->CreatePixelShader(
            skyPSBlob->GetBufferPointer(), skyPSBlob->GetBufferSize(),
            nullptr, &m_skyPS
        );

        // SkySphereの入力値作成
        D3D11_INPUT_ELEMENT_DESC skyLayout[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D11_INPUT_PER_VERTEX_DATA,0},
        };

        m_device->CreateInputLayout(
            skyLayout, 1,
            skyVSBlob->GetBufferPointer(), skyVSBlob->GetBufferSize(),
            &m_skyInputLayout);

        //-定数バッファを登録するためのラムダ式
        auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& buf)
            {
                D3D11_BUFFER_DESC d = {};
                d.ByteWidth = size;
                d.Usage = D3D11_USAGE_DYNAMIC;
                d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                m_device->CreateBuffer(&d, nullptr, &buf);
            };

        //-空(設定)用定数バッファの作成
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

        //==================================

        //-空用頂点バッファの作成
        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth = static_cast<UINT>(sizeof(glm::vec3) * verts.size());
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vd = {};
        vd.pSysMem = verts.data();
        m_device->CreateBuffer(&vbd, &vd, &m_skyVB);

        //-空用インデックスバッファの作成
        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * idxs.size());
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA id = {};
        id.pSysMem = idxs.data();
        m_device->CreateBuffer(&ibd, &id, &m_skyIB);

        LOG_INFO("SkySphere shaders initialized");
    }

    // EquirectToCubemap(スカイボックスで用いられる６面に分割した画像を用いたキューブマップ)
    {
        //-キューブマップシェーダーのパス変換
        std::filesystem::path cubeVSPath = PathResolver::resolve("assets/shaders/EquirectToCubemap.vert.hlsl");
        std::filesystem::path cubePSPath = PathResolver::resolve("assets/shaders/EquirectToCubemap.pixel.hlsl");

        if (!std::filesystem::exists(cubeVSPath))
        {
            LOG_ERROR("Cubemap vertex shader not found: {}", cubeVSPath.string());
            return false;
        }
        if (!std::filesystem::exists(cubePSPath))
        {
            LOG_ERROR("Cubemap pixel shader not found: {}", cubePSPath.string());
            return false;
        }

        ComPtr<ID3DBlob> cubeVSBlob, cubePSBlob, cubeErrBlob;

        //-Cubemap頂点シェーダーコンパイル
        hr = D3DCompileFromFile(
            cubeVSPath.wstring().c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &cubeVSBlob, &cubeErrBlob);
        if (FAILED(hr))
        {
            if (cubeErrBlob)
                LOG_ERROR("Cubemap VS error: {}", static_cast<char*>(cubeErrBlob->GetBufferPointer()));
            return false;
        }

        //-Cubemapピクセルシェーダーコンパイル
        hr = D3DCompileFromFile(
            cubePSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &cubePSBlob, &cubeErrBlob);
        if (FAILED(hr))
        {
            if (cubeErrBlob)
                LOG_ERROR("Cubemap PS error: {}", static_cast<char*>(cubeErrBlob->GetBufferPointer()));
            return false;
        }

        //-シェーダー作成
        m_device->CreateVertexShader(
            cubeVSBlob->GetBufferPointer(), cubeVSBlob->GetBufferSize(),
            nullptr, &m_cubemapVS);
        m_device->CreatePixelShader(
            cubePSBlob->GetBufferPointer(), cubePSBlob->GetBufferSize(),
            nullptr, &m_cubemapPS);
        //-Cubeの入力構造定義
        D3D11_INPUT_ELEMENT_DESC cubeLayout[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D11_INPUT_PER_VERTEX_DATA,0},
        };
        //-Cube入力構造登録
        m_device->CreateInputLayout(
            cubeLayout, 1,
            cubeVSBlob->GetBufferPointer(), cubeVSBlob->GetBufferSize(),
            &m_cubemapInputLayout);

        //-Cubeの定数バッファを作成
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth = sizeof(SkyCB);
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&cbd, nullptr, &m_cubemapCB);

        //-環境マッピングを作成
        m_environmentMap = std::make_unique<EnvironmentMap>();
        m_environmentMap->create(m_device.Get(), 512);

        LOG_INFO("EquirectToCubemap shaders initialized");
    }

    // Cubemap
    {
        // 単位立方体(内側から見る想定なので巻き順はスカイスフィアと合わせて逆巻きでOK)
        const glm::vec3 verts[8] = {
            {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
            {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
        };
        const uint32_t idx[36] = {
            0,1,2, 0,2,3,   // -Z
            5,4,7, 5,7,6,   // +Z
            4,0,3, 4,3,7,   // -X
            1,5,6, 1,6,2,   // +X
            3,2,6, 3,6,7,   // +Y
            4,5,1, 4,1,0,   // -Y
        };
        //-頂点バッファを作成
        D3D11_BUFFER_DESC vbd{}; vbd.ByteWidth = sizeof(verts); vbd.Usage = D3D11_USAGE_IMMUTABLE; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd{ verts };
        m_device->CreateBuffer(&vbd, &vsd, &m_cubemapVB);

        //-インデックスバッファを作成
        D3D11_BUFFER_DESC ibd{}; ibd.ByteWidth = sizeof(idx); ibd.Usage = D3D11_USAGE_IMMUTABLE; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd{ idx };
        m_device->CreateBuffer(&ibd, &isd, &m_cubemapIB);
    }

    // IrradianceConvolution(関節腔計算手法) 
    {
        //-シェーダーファイルパス変換
        std::filesystem::path irrPSPath = PathResolver::resolve("assets/shaders/IrradianceConvolution.pixel.hlsl");
        //-ファイル存在確認
        if (!std::filesystem::exists(irrPSPath))
        {
            LOG_ERROR("Irradiance pixel shader not found: {}", irrPSPath.string());
            return false;
        }

        ComPtr<ID3DBlob> irrPSBlob, irrErrBlob;
        //-シェーダーコンパイル
        hr = D3DCompileFromFile(
            irrPSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &irrPSBlob, &irrErrBlob);
        if (FAILED(hr))
        {
            if (irrErrBlob)
                LOG_ERROR("Irradiance PS error: {}", static_cast<char*>(irrErrBlob->GetBufferPointer()));
            return false;
        }
        //-ピクセルシェーダー作成
        m_device->CreatePixelShader(
            irrPSBlob->GetBufferPointer(), irrPSBlob->GetBufferSize(),
            nullptr, &m_irradiancePS);
        //-環境マップ作成
        m_irradianceMap = std::make_unique<EnvironmentMap>();
        m_irradianceMap->create(m_device.Get(), 32);

        LOG_INFO("IrradianceConvolution shader initialized");
    }

    // PrefilteredEnvironment(IBLに置ける粗さを計算に含む処理)
    {
        //-シェーダーファイルパスの変換
        std::filesystem::path prefPSPath = PathResolver::resolve("assets/shaders/PrefilterEnvironment.pixel.hlsl");
        //-ファイル存在確認
        if (!std::filesystem::exists(prefPSPath))
        {
            LOG_ERROR("Prefiltered pixel shader not found: {}", prefPSPath.string());
            return false;
        }

        ComPtr<ID3DBlob> prefPSBlob, prefErrBlob;
        //-シェーダーコンパイル
        hr = D3DCompileFromFile(
            prefPSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &prefPSBlob, &prefErrBlob);
        if (FAILED(hr))
        {
            if (prefErrBlob)
                LOG_ERROR("Prefilter PS error: {}", static_cast<char*>(prefErrBlob->GetBufferPointer()));
            return false;
        }
        //-シェーダー作成
        m_device->CreatePixelShader(
            prefPSBlob->GetBufferPointer(), prefPSBlob->GetBufferSize(),
            nullptr, &m_prefilterPS);
        //-バッファ作成
        D3D11_BUFFER_DESC pcbd = {};
        pcbd.ByteWidth = sizeof(PrefilterCB);
        pcbd.Usage = D3D11_USAGE_DYNAMIC;
        pcbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        pcbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&pcbd, nullptr, &m_prefilterCB);
        //-環境マップを作成
        m_prefilterMap = std::make_unique<EnvironmentMap>();
        m_prefilterMap->create(m_device.Get(), 128, 5);

        LOG_INFO("PrefilterEnvitornment shader initialized");
    }

    // BRDF LUT(PBRに置ける光の反射処理を行う)
    {
        //-シェーダーファイルパスの変換
        std::filesystem::path lutVSPath = PathResolver::resolve("assets/shaders/BRDFLUT.vert.hlsl");
        std::filesystem::path lutPSPath = PathResolver::resolve("assets/shaders/BRDFLUT.pixel.hlsl");
        //-ファイルの存在確認
        if (!std::filesystem::exists(lutVSPath))
        {
            LOG_ERROR("lut vertex shader not found: {}", lutVSPath.string());
            return false;
        }
        if (!std::filesystem::exists(lutPSPath))
        {
            LOG_ERROR("lut pixel shader not found: {}", lutPSPath.string());
            return false;
        }


        ComPtr<ID3DBlob> lutVSBlob, lutPSBlob, lutErrBlob;
        //-シェーダーコンパイル
        hr = D3DCompileFromFile(
            lutVSPath.wstring().c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &lutVSBlob, &lutErrBlob);
        if (FAILED(hr))
        {
            if (lutErrBlob)
                LOG_ERROR("BRDFLUT VS error: {}", static_cast<char*>(lutErrBlob->GetBufferPointer()));
            return false;
        }

        hr = D3DCompileFromFile(
            lutPSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &lutPSBlob, &lutErrBlob);
        if (FAILED(hr))
        {
            if (lutErrBlob)
                LOG_ERROR("BRDFLUT VS error: {}", static_cast<char*>(lutErrBlob->GetBufferPointer()));
            return false;
        }
        //-シェーダー作成
        m_device->CreateVertexShader(
            lutVSBlob->GetBufferPointer(), lutVSBlob->GetBufferSize(),
            nullptr, &m_brdfLutVS);
        m_device->CreatePixelShader(
            lutPSBlob->GetBufferPointer(), lutPSBlob->GetBufferSize(),
            nullptr, &m_brdfLutPS);
        //-LutTexture設定
        D3D11_TEXTURE2D_DESC lutDesc = {};
        lutDesc.Width = 512;
        lutDesc.Height = 512;
        lutDesc.MipLevels = 1;
        lutDesc.ArraySize = 1;
        lutDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
        lutDesc.SampleDesc.Count = 1;
        lutDesc.Usage = D3D11_USAGE_DEFAULT;
        lutDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        //-LutRender各種作成
        m_device->CreateTexture2D(&lutDesc, nullptr, &m_brdfLutTexture);
        m_device->CreateRenderTargetView(m_brdfLutTexture.Get(), nullptr, &m_brdfLutRTV);
        m_device->CreateShaderResourceView(m_brdfLutTexture.Get(), nullptr, &m_brdfLutSRV);

        LOG_INFO("BRDFLUT shaders initialized");
    }
    
    // Skinned PBR Vertex
    {
        //-ファイルパスの変換
        std::filesystem::path skinVSPath = PathResolver::resolve(
            "assets/shaders/PBR_Skinned.vert.hlsl"
        );
        if (!std::filesystem::exists(skinVSPath))
        {
            LOG_ERROR("Skinned vertex shader not found: {}", skinVSPath.string());
            return false;
        }
        ComPtr<ID3DBlob> skinVSBlob, skinErrBlob;
        //-シェーダーコンパイル   
        hr = D3DCompileFromFile(
            skinVSPath.wstring().c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &skinVSBlob, &skinErrBlob
        );
        if (FAILED(hr))
        {
            if (skinErrBlob)
                LOG_ERROR("Skinned VS error: {}", 
                    static_cast<char*>(skinErrBlob->GetBufferPointer()));
            return false;
        }
        //-シェーダー作成
        m_device->CreateVertexShader(
            skinVSBlob->GetBufferPointer(), skinVSBlob->GetBufferSize(),
            nullptr, &m_skinnedVertexShader
        );
        //-入力値構成
        D3D11_INPUT_ELEMENT_DESC skinLayout[] = {
            {"POSITION",    0,DXGI_FORMAT_R32G32B32_FLOAT,      0,offsetof(SkinnedVertex,position)},
            {"COLOR",       0,DXGI_FORMAT_R32G32B32A32_FLOAT,   0,offsetof(SkinnedVertex,color)},
            {"TEXCOORD",    0,DXGI_FORMAT_R32G32_FLOAT,         0,offsetof(SkinnedVertex,uv)},
            {"NORMAL",      0,DXGI_FORMAT_R32G32B32_FLOAT,      0,offsetof(SkinnedVertex,normal)},
            {"TANGENT",     0,DXGI_FORMAT_R32G32B32_FLOAT,      0,offsetof(SkinnedVertex,tangent)},
            {"BINORMAL",    0,DXGI_FORMAT_R32G32B32_FLOAT,      0,offsetof(SkinnedVertex,bitangent)},
            {"BONEINDICES", 0,DXGI_FORMAT_R32G32B32A32_SINT,   0,offsetof(SkinnedVertex,boneIndices)},
            {"BONEWEIGHTS", 0,DXGI_FORMAT_R32G32B32A32_FLOAT,   0,offsetof(SkinnedVertex,boneWeights)},
        };
        //-入力値作成
        hr = m_device->CreateInputLayout(
            skinLayout, ARRAYSIZE(skinLayout),
            skinVSBlob->GetBufferPointer(), skinVSBlob->GetBufferSize(),
            &m_skinnedInputLayout
        );
        if (FAILED(hr))
        {
            LOG_ERROR("CreateInputLayout (skinned) failed");
            return false;
        }
        //-空の定数バッファ作成
        D3D11_BUFFER_DESC skcbd = {};
        skcbd.ByteWidth = sizeof(SkinningCB);
        skcbd.Usage = D3D11_USAGE_DYNAMIC;
        skcbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        skcbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&skcbd, nullptr, &m_skinningCB);

        LOG_INFO("Skinned PBR Shader initialized");
    }

    // UI
    {
        //-ファイルパスの変換
        std::filesystem::path uiVSPath = PathResolver::resolve("assets/shaders/UI.vert.hlsl");
        std::filesystem::path uiPSPath = PathResolver::resolve("assets/shaders/UI.pixel.hlsl");
        //-ファイルの存在確認
        if (!std::filesystem::exists(uiVSPath))
        {
            LOG_ERROR("UI vertex shader not found: {}", uiVSPath.string());
            return false;
        }
        if (!std::filesystem::exists(uiPSPath))
        {
            LOG_ERROR("UI pixel shader not found: {}", uiPSPath.string());
            return false;
        }


        ComPtr<ID3DBlob> uiVSBlob, uiPSBlob, uiErrBlob;
        //-シェーダーコンパイル
        hr = D3DCompileFromFile(
            uiVSPath.wstring().c_str(),
            nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
            &uiVSBlob, &uiErrBlob
        );
        if (FAILED(hr))
        {
            if (uiErrBlob)
            {
                LOG_ERROR("UI VS error: {}", static_cast<char*>(uiErrBlob->GetBufferPointer()));
            }
            return false;
        }
        hr = D3DCompileFromFile(
            uiPSPath.wstring().c_str(),
            nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
            &uiPSBlob, &uiErrBlob
        );
        if (FAILED(hr))
        {
            if (uiErrBlob)
            {
                LOG_ERROR("UI PS error: {}", static_cast<char*>(uiErrBlob->GetBufferPointer()));
            }
            return false;
        }
        //-シェーダー作成
        m_device->CreateVertexShader(
            uiVSBlob->GetBufferPointer(),uiVSBlob->GetBufferSize(),
            nullptr,&m_uiVertexShader
        );
        m_device->CreatePixelShader(
            uiPSBlob->GetBufferPointer(), uiPSBlob->GetBufferSize(),
            nullptr, &m_uiPixelShader
        );
        //-入力値構成
        D3D11_INPUT_ELEMENT_DESC uiLayout[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,sizeof(glm::vec2),D3D11_INPUT_PER_VERTEX_DATA,0},
        };
        //-入力値作成
        m_device->CreateInputLayout(
            uiLayout, ARRAYSIZE(uiLayout),
            uiVSBlob->GetBufferPointer(), uiVSBlob->GetBufferSize(),
            &m_uiInputLayout
        );
        //-定数バッファ登録用ラムダ式
        auto makeCB = [&](UINT size, ComPtr<ID3D11Buffer>& buf)
            {
                D3D11_BUFFER_DESC d = {};
                d.ByteWidth = size;
                d.Usage = D3D11_USAGE_DYNAMIC;
                d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                m_device->CreateBuffer(&d, nullptr, &buf);
            };
        //-UI用定数バッファ
        makeCB(sizeof(UITransformCB), m_uiTransformCB);
        makeCB(sizeof(UIMaterialCB), m_uiMaterialCB);

        //-UI頂点情報
        struct UIVertex { glm::vec2 pos; glm::vec2 uv; };

        //======== Quad(四角形)
        UIVertex quadVerts[4] = {
            {{-0.5f, 0.5f},{ 0.0f, 0.0f}},
            {{ 0.5f, 0.5f},{ 1.0f, 0.0f}},
            {{ 0.5f,-0.5f},{ 1.0f, 1.0f}},
            {{-0.5f,-0.5f},{ 0.0f, 1.0f}},
        };
        uint32_t quadIndices[6] = { 0,1,2,0,2,3 };

        D3D11_BUFFER_DESC qvbd = {};
        qvbd.ByteWidth = sizeof(quadVerts);
        qvbd.Usage = D3D11_USAGE_IMMUTABLE;
        qvbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA qvData = {};
        qvData.pSysMem = quadVerts;
        m_device->CreateBuffer(&qvbd, &qvData, &m_uiQuadVB);

        D3D11_BUFFER_DESC qibd = {};
        qibd.ByteWidth = sizeof(quadIndices);
        qibd.Usage = D3D11_USAGE_IMMUTABLE;
        qibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA qiData = {};
        qiData.pSysMem = quadIndices;
        m_device->CreateBuffer(&qibd, &qiData, &m_uiQuadIB);
        //===========================

        //-ブレンディング設定
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        m_device->CreateBlendState(&bd, &m_uiBlendState);

        //-UI用深度バッファ作成
        D3D11_DEPTH_STENCIL_DESC uiDsd = {};
        uiDsd.DepthEnable = FALSE;
        uiDsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        m_device->CreateDepthStencilState(&uiDsd, &m_uiDepthState);

        //-UI用ラスタライザ作成
        D3D11_RASTERIZER_DESC uiRd = {};
        uiRd.FillMode = D3D11_FILL_SOLID;
        uiRd.CullMode = D3D11_CULL_NONE;
        uiRd.DepthClipEnable = TRUE;
        m_device->CreateRasterizerState(&uiRd, &m_uiRasterizerState);

        LOG_INFO("UI shaders initialized");
    }

    LOG_INFO("Shaders loaded: {} / {}", vsPath.string(), psPath.string());
    return true;
}

bool DX11Renderer::createDefaultStates()
{
    //-ラスタライザ作成
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    m_device->CreateRasterizerState(&rd, &m_rasterizerState);

    //-深度バッファ作成
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    m_device->CreateDepthStencilState(&dsd, &m_depthStencilState);

    //-シャドウサンプラー作成
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

    //-Sky深度バッファ作成
    D3D11_DEPTH_STENCIL_DESC skyDSD = {};
    skyDSD.DepthEnable = TRUE;
    skyDSD.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyDSD.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_device->CreateDepthStencilState(&skyDSD, &m_skyDepthState);

    // BRDF lutサンプラー作成
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
    //-サイズと深さの更新
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &vp);
}

void DX11Renderer::restoreMainRenderTarget()
{
    //-SceneRTが有効な場合SceneRTをバインド
    if (m_offscreen && m_sceneRT && m_sceneRT->isValid())
    {
        m_sceneRT->bindAsRenderTarget(m_context.Get());
    }
    //-GameRTが有効な場合GameRTをバインド
    else if (m_gameOffscreen && m_gameRT && m_gameRT->isValid())
    {
        m_gameRT->bindAsRenderTarget(m_context.Get());
    }
    //-
    else
    {
        m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
        updateViewport();
    }
}

void DX11Renderer::drawGrid(const glm::mat4& view, const glm::mat4& proj, float gridSize)
{

}

void DX11Renderer::updateLights(const LightCB& lightData)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    //-ライトに受け渡す定数を更新
    m_context->Map(m_lightCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &lightData, sizeof(LightCB));
    m_context->Unmap(m_lightCB.Get(), 0);
}

void DX11Renderer::shutdown() {
    if (m_context) m_context->ClearState();
    LOG_INFO("DX11Renderer shutdown");
}

void DX11Renderer::beginFrame() {
    //-
    m_context->ClearRenderTargetView(m_rtv.Get(), m_clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    //-
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    //-
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
    //-バックバッファの情報をフロントバッファに置き換える
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
    //-レンダーターゲットなどの描画情報を一度リセット
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    m_depthBuffer.Reset();
    //-バッファをリサイズ
    m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    //-再度レンダー関連を更新した情報で作成
    createRenderTargetView();
    createDepthStencilView();
    updateViewport();
    //-アスペクト比を計算し行列を作成
    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    m_projection = glm::perspectiveLH(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
}


glm::vec3 DX11Renderer::getCameraPosition() const
{
    glm::mat4  invView = glm::inverse(m_view);
    return invView[3];
}

void DX11Renderer::drawMesh(const Vertex* vertices, uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount, const glm::mat4& transform)
{
    //-頂点バッファ情報を構築
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(Vertex) * vertexCount;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vData = {};
    vData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vb;
    m_device->CreateBuffer(&vbd, &vData, &vb);

    //-インデックスバッファ情報を構築
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(uint32_t) * indexCount;
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iData = {};
    iData.pSysMem = indices;

    ComPtr<ID3D11Buffer> ib;
    m_device->CreateBuffer(&ibd, &iData, &ib);
 
    //-行列を計算
    glm::mat4 mvp = m_projection * m_view * transform;

    //-TransformCBを直接書き換えるためキャッシュ無効
    m_transformCBValid = false;
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    glm::mat4 mvpT = glm::transpose(mvp);
    memcpy(mapped.pData, &mvpT, sizeof(glm::mat4));
    m_context->Unmap(m_transformCB.Get(), 0);
    //-作成したバッファを設定
    UINT stride = sizeof(Vertex), offset = 0;
    m_context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    //-ドローコール
    m_context->DrawIndexed(indexCount, 0, 0);
}

namespace
{
    /// @brief マテリアルアセットが保持している情報を定数バッファに移す関数
    /// @param mat マテリアル定数バッファ
    /// @param material マテリアルアセット
    void fillMaterialCB(MaterialCB& mat, const MaterialAsset* material)
    {
        if (!material) return;

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

    /// @brief マテリアルのテクスチャのみをBindする関数
    /// @param ctx 
    /// @param mat 
    /// @param material 
    void bindMaterialTextures(ID3D11DeviceContext* ctx, const MaterialCB& mat, const MaterialAsset* material)
    {
        ID3D11ShaderResourceView* srvs[5] = { nullptr,nullptr,nullptr,nullptr,nullptr };
        if (material)
        {// 使用するMapのみBindする
            if (mat.useAlbedoMap)       srvs[0] = material->cachedAlbedoMap->srv.Get();
            if (mat.useMetallicMap)     srvs[1] = material->cachedMetallicMap->srv.Get();
            if (mat.useNormalMap)       srvs[2] = material->cachedNormalMap->srv.Get();
            if (mat.useAOMap)           srvs[3] = material->cachedAOMap->srv.Get();
            if (mat.useEmissiveMap)     srvs[4] = material->cachedEmissiveMap->srv.Get();
        }
        //-BindしたMapをシェーダーリソースに流す
        ctx->PSSetShaderResources(0, 5, srvs);
    }
} // namespace


void DX11Renderer::bindPBRFrameResources()
{
    // 一回の描画で変更されないものは不必要にBindしたり全解除したりしない
    if (m_dirShadowMap && m_dirShadowMap->isValid())
        m_dirShadowMap->bindForRead(m_context.Get(), 5);

    ID3D11ShaderResourceView* iblSRVs[3] =
    {
        m_irradianceMap ? m_irradianceMap->getSRV() : nullptr,
        m_prefilterMap ? m_prefilterMap->getSRV() : nullptr,
        m_brdfLutSRV.Get()
    };
    m_context->PSSetShaderResources(6, 3, iblSRVs);

    ID3D11ShaderResourceView* env = m_environmentMap ? m_environmentMap->getSRV() : nullptr;
    m_context->PSSetShaderResources(9, 1, &env);

    m_context->PSSetSamplers(2, 1, m_lutSampler.GetAddressOf());

    // ほかのパスが状態を抱えている可能性を考慮しキャッシュは無効かしておく
    m_boundVS = nullptr;
    m_boundPS = nullptr;
    m_boundLayout = nullptr;
    m_materialCBValid = false;
    m_transformCBValid = false;
}

/// @brief 
void FaluEngine::DX11Renderer::unbindPBRFrameResources()
{
    // SRV/RTVの同時バインド防止のためIBL/環境マップ/シャドウを書く込み処理の前に外す
    ID3D11ShaderResourceView* nulls[10] = {};
    m_context->PSSetShaderResources(5, 5, nulls);
    m_boundLayout = nullptr;
}

void FaluEngine::DX11Renderer::uploadMaterialCB(const MaterialCB& mat)
{
    // マテリアルが定数バッファが無効だったり内容が一致していたら早期リターン
    if (m_materialCBValid && std::memcmp(&m_lastMaterialCB, &mat, sizeof(MaterialCB)) == 0)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    // 
    if (FAILED(m_context->Map(m_materialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.pData, &mat, sizeof(MaterialCB));
    m_context->Unmap(m_materialCB.Get(), 0);

    // キャッシュを作成
    m_lastMaterialCB = mat;
    m_materialCBValid = true;
}

void FaluEngine::DX11Renderer::uploadTransformCB(const glm::mat4& world)
{
    TransformKey key = { world,m_view,m_projection };
    // 定数バッファが無効だったり行列が一致していたら早期リターン
    if (m_transformCBValid && std::memcmp(&m_lastTransformKey, &key, sizeof(TransformKey)) == 0)
        return;

    TransformCB cb;
    cb.mvp = glm::transpose(m_projection * m_view * world);
    cb.world = glm::transpose(world);
    cb.normalMatrix = glm::inverse(world);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(m_context->Map(m_transformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.pData, &cb, sizeof(TransformCB));
    m_context->Unmap(m_transformCB.Get(), 0);

    m_lastTransformKey = key;
    m_transformCBValid = true;
}

void DX11Renderer::drawSubMeshPBR(uint32_t indexOffset, uint32_t indexCount,
    const glm::mat4& transform,
    struct MaterialAsset* material)
{
    //-カスタムシェーダーが存在するか確認
    bool hasCustom = material && material->cachedShader &&
        material->cachedShader->valid;

    //-カスタムシェーダーがある場合、マテリアルに付与されているシェーダーを取得
    ID3D11VertexShader* vs = hasCustom
        ? material->cachedShader->vertexShader.Get() : m_vertexShader.Get();
    ID3D11PixelShader* ps = hasCustom
        ? material->cachedShader->pixelShader.Get() : m_pixelShader.Get();
    ID3D11InputLayout* layout = hasCustom
        ? material->cachedShader->inputLayout.Get() : m_inputLayout.Get();

    if (m_boundVS != vs) { m_context->VSSetShader(vs, nullptr, 0); m_boundVS = vs; }
    if (m_boundPS != ps) { m_context->PSSetShader(ps, nullptr, 0); m_boundPS = ps; }
    if (m_boundLayout != layout) { m_context->IASetInputLayout(layout); m_boundLayout = layout; }

    //==== Material Parameter =========
    MaterialCB mat;
    //-マテリアルを再構築
    fillMaterialCB(mat, material);
    uploadMaterialCB(mat);
    uploadTransformCB(transform);
    bindMaterialTextures(m_context.Get(), mat, material);
    //-ドローコール
    m_context->DrawIndexed(indexCount, indexOffset, 0);
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
    m_activeWidth = width;
    m_activeHeight = height;

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
    m_activeWidth = m_width;
    m_activeHeight = m_height;

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
    m_activeWidth = width;
    m_activeHeight = height;

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
    m_activeWidth = m_width;
    m_activeHeight = m_height;

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

void DX11Renderer::bindShadowVertexStage(bool skinned)
{
    if (skinned)
    {
        m_context->IASetInputLayout(m_shadowSkinnedInputLayout.Get());
        m_context->VSSetShader(m_shadowSkinnedVS.Get(), nullptr, 0);
    }
    else
    {
        m_context->IASetInputLayout(m_shadowInputLayout.Get());
        m_context->VSSetShader(m_shadowVS.Get(), nullptr, 0);
    }
}

void DX11Renderer::beginShadowPass(const glm::mat4& lightView, const glm::mat4& lightProj)
{
    m_dirShadowMap->lightView = lightView;
    m_dirShadowMap->lightProjection = lightProj;
    m_lightVP = lightProj * lightView;
    m_dirShadowMap->unbind(m_context.Get(), 5);
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

    // シェーダーを通常描画用に戻す
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
    cb.lightMVP = glm::transpose(m_lightVP * world);
    
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

void DX11Renderer::updateSkinningMatrices(const std::vector<glm::mat4>& boneMatrices)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if(FAILED(m_context->Map(m_skinningCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    // 一時バッファを経由せず、マップ先へ直接書き込む
    auto* dst = static_cast<glm::mat4*>(mapped.pData);
    const size_t count = (std::min)(boneMatrices.size(), static_cast<size_t>(MAX_BONES));
    for (size_t i = 0; i < count;++i)
    {
        dst[i] = glm::transpose(boneMatrices[i]);
    }
    for (size_t i = count; i < MAX_BONES; ++i)
    {
        dst[i] = glm::mat4(1.0f);
    }

    m_context->Unmap(m_skinningCB.Get(), 0);

    m_context->VSSetConstantBuffers(4, 1, m_skinningCB.GetAddressOf());
}

void DX11Renderer::drawSkinnedSubMeshPBR(uint32_t indexOffset, uint32_t indexCount, const glm::mat4& transform, MaterialAsset* material)
{
    ID3D11PixelShader* ps = (material && material->cachedShader && material->cachedShader->valid)
        ? material->cachedShader->pixelShader.Get() : m_pixelShader.Get();

    if (m_boundVS != m_skinnedVertexShader.Get())
    {
        m_context->VSSetShader(m_skinnedVertexShader.Get(), nullptr,0);
        m_boundVS = m_skinnedVertexShader.Get();
    }
    if (m_boundPS != ps)
    {
        m_context->PSSetShader(ps, nullptr, 0);
        m_boundPS = ps;
    }
    if (m_boundLayout != m_skinnedInputLayout.Get())
    {
        m_context->IASetInputLayout(m_skinnedInputLayout.Get());
        m_boundLayout = m_skinnedInputLayout.Get();
    }

    MaterialCB mat;
    fillMaterialCB(mat, material);
    uploadMaterialCB(mat);
    uploadTransformCB(transform);
    bindMaterialTextures(m_context.Get(), mat, material);

    m_context->DrawIndexed(indexCount, indexOffset, 0);
}

//======== UI ============

void DX11Renderer::drawUIQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color, ID3D11ShaderResourceView* srv)
{
    UITransformCB tcb;
    tcb.orthoProjection = glm::transpose(m_uiOrthoProjection);
    tcb.position = position;
    tcb.size = size;
    tcb.rotation = glm::radians(rotation);

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_uiTransformCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &tcb, sizeof(UITransformCB));
    m_context->Unmap(m_uiTransformCB.Get(), 0);

    UIMaterialCB mcb;
    mcb.color = color;
    mcb.useTexture = srv ? 1 : 0;

    m_context->Map(m_uiMaterialCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &mcb, sizeof(UIMaterialCB));
    m_context->Unmap(m_uiMaterialCB.Get(),0);

    if (srv) 
        m_context->PSSetShaderResources(0, 1, &srv);

    m_context->DrawIndexed(6, 0, 0);

    if (srv)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
    }
}

void DX11Renderer::beginUIPass(uint32_t screenWidth, uint32_t screenHeight)
{
    m_uiOrthoProjection = glm::orthoLH(
        -static_cast<float>(screenWidth) * 0.5f, static_cast<float>(screenWidth) * 0.5f,
        -static_cast<float>(screenHeight) * 0.5f, static_cast<float>(screenHeight) * 0.5f,
        -1.0f, 1.0f
    );

    m_context->IASetInputLayout(m_uiInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_uiVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_uiPixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_uiTransformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_uiMaterialCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    UINT stride = sizeof(glm::vec2) * 2, offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_uiQuadVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_uiQuadIB.Get(), DXGI_FORMAT_R32_UINT, 0);

    float blendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
    m_context->OMSetBlendState(m_uiBlendState.Get(), blendFactor, 0xffffffff);
    m_context->OMSetDepthStencilState(m_uiDepthState.Get(), 0);
    m_context->RSSetState(m_uiRasterizerState.Get());

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;
}

void DX11Renderer::endUIPass()
{
    m_context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());

    m_boundVB = nullptr;
    m_boundIB = nullptr;
    m_boundVS = nullptr;
    m_boundPS = nullptr;

    // シェーダーを通常描画用に戻す
    m_context->VSSetConstantBuffers(0, 1, m_transformCB.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_materialCB.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_lightCB.GetAddressOf());
}

//=================================

void DX11Renderer::generateEnvironmentMap(const SkySettingsCB& settings, ID3D11ShaderResourceView* skySRV)
{
    unbindPBRFrameResources();
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
    m_context->IASetVertexBuffers(0, 1, m_cubemapVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_cubemapIB.Get(), DXGI_FORMAT_R32_UINT, 0);

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

        m_context->DrawIndexed(36, 0, 0);
    }

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());
    updateViewport();

    if (skySRV)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
    }

    restoreMainRenderTarget();

    LOG_INFO("EnvironmentMap generated from SkySphere");
}

void DX11Renderer::generateIrradianceMap()
{
    unbindPBRFrameResources();
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

    restoreMainRenderTarget();

    LOG_INFO("IrradianceMap generated ({}x{} x6)", size, size);
}

void DX11Renderer::generatePrefilterMap()
{
    unbindPBRFrameResources();
    if (!m_prefilterMap || !m_environmentMap)
    {
        LOG_ERROR("PrefilterMap: missing prefilterMap or environmentMap");
        return;
    }

    auto* envSRV = m_environmentMap->getSRV();
    if (!envSRV)
    {
        LOG_ERROR("PrefilterMap: environmentMap SRV is null");
        return;
    }
    LOG_INFO("PrefilterMap: envSRV is valid, starting generation");

    m_context->IASetInputLayout(m_cubemapInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_cubemapVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_prefilterPS.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_cubemapCB.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_prefilterCB.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, &envSRV);

    UINT stride = sizeof(glm::vec3), offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyVB.GetAddressOf(), &stride, &offset);
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

    restoreMainRenderTarget();

    LOG_INFO("PrefilterMap generated ({} mips)", maxMips);
}

void DX11Renderer::generateBRDFLUT()
{
    unbindPBRFrameResources();
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

    restoreMainRenderTarget();
    LOG_INFO("BRDFLUT generated (512x512)");
}

} // namespace FaluEngine
