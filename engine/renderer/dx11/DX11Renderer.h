#pragma once
#include "renderer/IRenderer.h"

#ifdef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <string>
#include "RenderTexture.h"
#include "ShadowMap.h"
#include <memory>

using Microsoft::WRL::ComPtr;

namespace FaluEngine {

struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

enum class LightType : int
{
    Directional = 0,
    Point = 1,
    Spot = 2,
};

struct LightData {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 color;
    int type;
    float range;
    float spotInner;
    float spotOuter;
};

struct LightCB {
    LightData lights[16];
    int lightCount = 0;
    float _pad0[3];
    glm::vec3 cameraPos = {};
    float _pad1;
    glm::vec4 ambientColor = { 0.2f,0.2f,0.2f,1.0f };
};

struct TransformCB {
    glm::mat4 mvp;
    glm::mat4 world;
    glm::mat4 normalMatrix;
};

struct MaterialCB {
    int useTexture = 0;
    int useNormalMap = 0;
    float shininess = 32.0f;
    float _pad = 0.0f;
};

struct ShadowCB
{
    glm::mat4 lightMVP;
};

struct ShadowSettingsCB
{
    glm::mat4 lightSpaceMatrix;
    int useShadow = 0;
    int useSoftShadow = 0;
    float shadowBias = 0.005f;
    float pcfRadius = 1.5f;
};



class DX11Renderer final : public IRenderer {
public:
    DX11Renderer()  = default;
    ~DX11Renderer() override { shutdown(); }

    bool init(void* windowHandle, uint32_t width, uint32_t height) override;
    void shutdown() override;

    void beginFrame() override;
    void endFrame()   override;

    void updateLights(const LightCB& lightData);

    void renderScene(const Scene& scene) override;
    void onResize(uint32_t width, uint32_t height) override;

    [[nodiscard]] uint32_t getWidth()  const noexcept override { return m_width; }
    [[nodiscard]] uint32_t getHeight() const noexcept override { return m_height; }

    // DirectX オブジェクトへの直接アクセス（他サブシステムから使う場合）
    [[nodiscard]] ID3D11Device*        getDevice()  const noexcept { return m_device.Get(); }
    [[nodiscard]] ID3D11DeviceContext* getContext() const noexcept { return m_context.Get(); }

    [[nodiscard]] ID3D11Buffer* getBoundVB() const noexcept { return m_boundVB; }
    [[nodiscard]] ID3D11Buffer* getBoundIB() const noexcept { return m_boundIB; }
    void setBoundVB(ID3D11Buffer* vb)noexcept { m_boundVB = vb; }
    void setBoundIB(ID3D11Buffer* ib)noexcept { m_boundIB = ib; }

    void drawMesh(const Vertex* vertices, uint32_t vertexCount,
        const uint32_t* indices, uint32_t indexCount,
        const glm::mat4& transform
    );
    void drawSubMesh(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& transform);

    void drawSubMeshTextured(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& transform,
        ID3D11ShaderResourceView* srv,
        ID3D11ShaderResourceView* normalSRV = nullptr);


    void setClearColor(float r, float g, float b, float a = 1.0f) {
        m_clearColor[0] = r; m_clearColor[1] = g;
        m_clearColor[2] = b; m_clearColor[3] = a;
    }

    void beginOffscreen(uint32_t width, uint32_t height);
    void endOffscreen();

    [[nodiscard]] ID3D11ShaderResourceView* getSceneSRV() const noexcept {
        return m_sceneRT ? m_sceneRT->getSRV() : nullptr;
    }
    [[nodiscard]] bool isOffscreen() const noexcept { return m_offscreen; }

    void setViewProjection(const glm::mat4& view, const glm::mat4& projection) {
        m_view = view;
        m_projection = projection;
    }

    void beginShadowPass(const glm::mat4& lightView, const glm::mat4& lightProj);
    void endShadowPass();

    void drawShadowMesh(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& world);
    
    void updateShadowSettings(const ShadowSettingsCB& settings);
    
    [[nodiscard]] ShadowMap* getDirShadowMap() const noexcept {
        return m_dirShadowMap.get();
    }

private:
    bool createDeviceAndSwapChain(HWND hwnd);
    bool createRenderTargetView();
    bool createDepthStencilView();
    bool createShaders(const std::string& vsPath,const std::string& psPath);
    bool createDefaultStates();

    void updateViewport();

private:
    ComPtr<ID3D11Device>            m_device;
    ComPtr<ID3D11DeviceContext>     m_context;
    ComPtr<IDXGISwapChain>          m_swapChain;
    ComPtr<ID3D11RenderTargetView>  m_rtv;
    ComPtr<ID3D11DepthStencilView>  m_dsv;
    ComPtr<ID3D11Texture2D>         m_depthBuffer;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    ComPtr<ID3D11Buffer> m_transformCB;
    ComPtr<ID3D11Buffer> m_materialCB;
    ComPtr<ID3D11Buffer> m_lightCB;
    ComPtr<ID3D11Buffer> m_shadowCB;
    ComPtr<ID3D11Buffer> m_shadowSettingsCB;
    ComPtr<ID3D11VertexShader> m_shadowVS;
    ComPtr<ID3D11InputLayout> m_shadowInputLayout;
    ComPtr<ID3D11RasterizerState> m_shadowRasterizerState;
    std::unique_ptr<ShadowMap> m_dirShadowMap;

    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    ID3D11Buffer* m_boundVB = nullptr;
    ID3D11Buffer* m_boundIB = nullptr;
    std::unique_ptr<RenderTexture> m_sceneRT;
    bool m_offscreen = false;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    bool     m_vsync  = true;

    float m_clearColor[4] = { 0.18f, 0.18f, 0.20f, 1.0f };

    glm::mat4 m_view = glm::mat4(1.0f);
    glm::mat4 m_projection = glm::mat4(1.0f);

};

} // namespace FaluEngine
