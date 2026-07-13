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
#include <memory>
#include <vector>
#include "RenderTexture.h"
#include "ShadowMap.h"
#include "EnvironmentMap.h"
#include "asset/loaders/ShaderLoader.h"
#include "asset/loaders/MaterialLoader.h"
#include "asset/loaders/SkeletonType.h"

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

struct SkinnedVertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::ivec4 boneIndices = { -1,-1,-1,-1 };
    glm::vec4 boneWeights = { 0.0f,0.0f,0.0f,0.0f };
};
#define MAX_BONES (128)
struct SkinningCB
{
    glm::mat4 boneMatrices[MAX_BONES];
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
    glm::vec4 albedoColor = { 1.0f,1.0f,1.0f,1.0f };
    float metallic = 0.0f;
    float roughness = 0.5f;
    int useAlbedoMap = 0;
    int useMetallicMap = 0;
    int useNormalMap = 0;
    int useAOMap = 0;
    int useEmissiveMap = 0;
    float emissiveStrength = 1.0f;
    glm::vec3 emissiveColor = { 0.0f,0.0f,0.0f };
    float _matPad = 0.0f;
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

struct SkyCB
{
    glm::mat4 viewProj;
};

struct SkySettingsCB
{
    glm::vec4 topColor;
    glm::vec4 bottomColor;
    glm::vec4 horizonColor;
    int useTexture = 0;
    float exposure = 1.0f;
    float _pad[2] = {};
};

struct PrefilterCB
{
    float roughness = 0.0f;
    glm::vec3 _pad = {};
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
    [[nodiscard]] const glm::mat4& getView() const noexcept { return m_view; }
    [[nodiscard]] const glm::mat4& getProjection() const noexcept { return m_projection; }

    [[nodiscard]] ID3D11Buffer* getBoundVB() const noexcept { return m_boundVB; }
    [[nodiscard]] ID3D11Buffer* getBoundIB() const noexcept { return m_boundIB; }
    void setBoundVB(ID3D11Buffer* vb)noexcept { m_boundVB = vb; }
    void setBoundIB(ID3D11Buffer* ib)noexcept { m_boundIB = ib; }
    [[nodiscard]] ID3D11VertexShader* getBoundVS() const noexcept { return m_boundVS; }
    [[nodiscard]] ID3D11PixelShader* getBoundPS() const noexcept { return m_boundPS; }

    void drawMesh(const Vertex* vertices, uint32_t vertexCount,
        const uint32_t* indices, uint32_t indexCount,
        const glm::mat4& transform
    );
    void drawSubMeshPBR(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& transform,
        class MaterialAsset* material);

    void drawSkySphere(const glm::mat4& view, const glm::mat4& proj,
        const SkySettingsCB& settings,
        ID3D11ShaderResourceView* srv = nullptr);

    void setClearColor(float r, float g, float b, float a = 1.0f) {
        m_clearColor[0] = r; m_clearColor[1] = g;
        m_clearColor[2] = b; m_clearColor[3] = a;
    }

    void beginOffscreen(uint32_t width, uint32_t height);
    void endOffscreen();

    // Scene View
    [[nodiscard]] ID3D11ShaderResourceView* getSceneSRV() const noexcept {
        return m_sceneRT ? m_sceneRT->getSRV() : nullptr;
    }
    [[nodiscard]] bool isOffscreen() const noexcept { return m_offscreen; }

    // Game View
    void beginGameOffscreen(uint32_t width, uint32_t height);
    void endGameOffscreen();

    [[nodiscard]] ID3D11ShaderResourceView* getGameSceneSRV() const noexcept {
        return m_gameRT ? m_gameRT->getSRV() : nullptr;
    }

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

    // Skinning
    void updateSkinningMatrices(const std::vector<glm::mat4>& boneMatrices);
    void drawSkinnedSubMeshPBR(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& transform, class MaterialAsset* material);


    void generateEnvironmentMap(const SkySettingsCB& settings,
        ID3D11ShaderResourceView* skySRV);
    void generateIrradianceMap();
    void generatePrefilterMap();
    void generateBRDFLUT();

    [[nodiscard]] ID3D11ShaderResourceView* getEnvironmentMapSRV() const noexcept
    {
        return m_environmentMap ? m_environmentMap->getSRV() : nullptr;
    }
    [[nodiscard]] ID3D11ShaderResourceView* getIrradianceMapSRV() const noexcept
    {
        return m_irradianceMap ? m_irradianceMap->getSRV() : nullptr;
    }
    [[nodiscard]] ID3D11ShaderResourceView* getPrefilterMapSRV() const noexcept {
        return m_prefilterMap ? m_prefilterMap->getSRV() : nullptr;
    }
    [[nodiscard]] ID3D11ShaderResourceView* getBRDFLutSRV() const noexcept {
        return m_brdfLutSRV.Get();
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
    ComPtr<ID3D11SamplerState> m_lutSampler;

    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    // EnvironmentMap
    std::unique_ptr<EnvironmentMap> m_environmentMap;
    ComPtr<ID3D11VertexShader> m_cubemapVS;
    ComPtr<ID3D11PixelShader> m_cubemapPS;
    ComPtr<ID3D11InputLayout> m_cubemapInputLayout;
    ComPtr<ID3D11Buffer> m_cubemapCB;

    // IrradianceMap
    std::unique_ptr<EnvironmentMap> m_irradianceMap;
    ComPtr<ID3D11VertexShader> m_irradianceVS;
    ComPtr<ID3D11PixelShader> m_irradiancePS;
    ComPtr<ID3D11InputLayout> m_irradianceInputLayout;

    // Prefilter
    std::unique_ptr<EnvironmentMap> m_prefilterMap;
    ComPtr<ID3D11PixelShader> m_prefilterPS;
    ComPtr<ID3D11Buffer> m_prefilterCB;

    // BRDFLut
    ComPtr<ID3D11VertexShader> m_brdfLutVS;
    ComPtr<ID3D11PixelShader> m_brdfLutPS;
    ComPtr<ID3D11Texture2D> m_brdfLutTexture;
    ComPtr<ID3D11RenderTargetView> m_brdfLutRTV;
    ComPtr<ID3D11ShaderResourceView> m_brdfLutSRV;

    // SkySphere
    ComPtr<ID3D11VertexShader> m_skyVS;
    ComPtr<ID3D11PixelShader> m_skyPS;
    ComPtr<ID3D11InputLayout> m_skyInputLayout;
    ComPtr<ID3D11Buffer> m_skyCB;
    ComPtr<ID3D11Buffer> m_skySettingsCB;
    ComPtr<ID3D11Buffer> m_skyVB;
    ComPtr<ID3D11Buffer> m_skyIB;
    ComPtr<ID3D11DepthStencilState> m_skyDepthState;
    uint32_t m_skyIndexCount = 0;

    // SkinMesh
    ComPtr<ID3D11VertexShader> m_skinnedVertexShader;
    ComPtr<ID3D11InputLayout> m_skinnedInputLayout;
    ComPtr<ID3D11Buffer> m_skinningCB;

    ID3D11Buffer* m_boundVB = nullptr;
    ID3D11Buffer* m_boundIB = nullptr;
    ID3D11VertexShader* m_boundVS = nullptr;
    ID3D11PixelShader* m_boundPS = nullptr;
    std::unique_ptr<RenderTexture> m_sceneRT;
    std::unique_ptr<RenderTexture> m_gameRT;
    bool m_offscreen = false;
    bool m_gameOffscreen = false;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    bool     m_vsync  = true;

    float m_clearColor[4] = { 0.18f, 0.18f, 0.20f, 1.0f };

    glm::mat4 m_view = glm::mat4(1.0f);
    glm::mat4 m_projection = glm::mat4(1.0f);

};

} // namespace FaluEngine
