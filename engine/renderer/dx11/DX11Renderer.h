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
#include <FaluEngine/EngineExport.h>

using Microsoft::WRL::ComPtr;

namespace FaluEngine {

/// @brief 頂点情報
struct FALU_ENGINE_API Vertex {
    glm::vec3 position;     // 位置
    glm::vec4 color;        // 色
    glm::vec2 uv;           // UV座標
    glm::vec3 normal;       // 法線
    glm::vec3 tangent;      // 接線
    glm::vec3 bitangent;    // 従接線ベクトル
};

/// @brief スキンメッシュの頂点
struct FALU_ENGINE_API SkinnedVertex
{
    glm::vec3 position;     // 位置
    glm::vec4 color;        // 色
    glm::vec2 uv;           // UV座標
    glm::vec3 normal;       // 法線
    glm::vec3 tangent;      // 接線
    glm::vec3 bitangent;    // 従接線ベクトル
    glm::ivec4 boneIndices = { -1,-1,-1,-1 };           // 骨のインデックス(接続する物も保持)
    glm::vec4 boneWeights = { 0.0f,0.0f,0.0f,0.0f };    // 骨の重さ
};
#define MAX_BONES (128) // 最大の骨の数
/// @brief スキンメッシュの定数バッファ
struct FALU_ENGINE_API SkinningCB
{
    glm::mat4 boneMatrices[MAX_BONES]; // 骨の行列
};

/// @brief ライトのデータ
struct FALU_ENGINE_API LightData {
    glm::vec4 position;     // 位置
    glm::vec4 direction;    // 方向
    glm::vec4 color;        // 色
    int type;               //-ライトの種類
    float range;            //-効果範囲
    float spotInner;        //-光がまっすぐどく円の範囲
    float spotOuter;        //-光がなくなる円の半径位置
};
/// @brief ライトの定数バッファ
struct FALU_ENGINE_API LightCB {
    LightData lights[16];       // ライトのデータ(最大16個まで)
    int lightCount = 0;         // ライトの個数
    float _pad0[3];             // 
    glm::vec3 cameraPos = {};   // カメラの位置
    float _pad1;
    glm::vec4 ambientColor = { 0.2f,0.2f,0.2f,1.0f }; // 環境色
};

/// @brief トランスフォーム定数バッファ
struct FALU_ENGINE_API TransformCB {
    glm::mat4 mvp;          // ビュープロジェクション行列
    glm::mat4 world;        // ワールド行列
    glm::mat4 normalMatrix; // 法線行列
};

/// @brief マテリアル定数バッファ
struct FALU_ENGINE_API MaterialCB {
    glm::vec4 albedoColor = { 1.0f,1.0f,1.0f,1.0f }; // ベースカラー
    float metallic = 0.0f;          // 金属
    float roughness = 0.5f;         //-粗さ
    int useAlbedoMap = 0;           //-アルベドマップ使用フラグ
    int useMetallicMap = 0;         //-メタリックマップ使用フラグ
    int useNormalMap = 0;           //-法線マップ使用フラグ
    int useAOMap = 0;               //-AOマップ(環境影マップ)の使用フラグ
    int useEmissiveMap = 0;         //-自己発光マップの使用フラグ
    float emissiveStrength = 1.0f;  //-発光強度
    glm::vec3 emissiveColor = { 0.0f,0.0f,0.0f };   //-発光色
    float _matPad = 0.0f;           //-
};

/// @brief 影定数バッファ
struct FALU_ENGINE_API ShadowCB
{
    glm::mat4 lightMVP; //-ライトの行列
};

/// @brief 影設定の定数バッファ
struct FALU_ENGINE_API ShadowSettingsCB
{
    glm::mat4 lightSpaceMatrix; //-ライト行列
    int useShadow = 0;          //-影の使用フラグ
    int useSoftShadow = 0;      //-ソフトシャドウの使用フラグ
    float shadowBias = 0.005f;  //-影の強度
    float pcfRadius = 1.5f;     //-ソフトシャドウの半径
};

/// @brief スカイボックス定数バッファ
struct FALU_ENGINE_API SkyCB
{
    glm::mat4 viewProj; //-ビュープロジェクション行列
};

/// @brief スカイボックス設定定数バッファ
struct FALU_ENGINE_API SkySettingsCB
{
    glm::vec4 topColor;     //-頂点色
    glm::vec4 bottomColor;  //-底辺色
    glm::vec4 horizonColor; //-並行色
    int useTexture = 0;     //-テクスチャの使用フラグ
    float exposure = 1.0f;  //-空の明るさ
    float _pad[2] = {};     //-
};

/// @brief 
struct FALU_ENGINE_API PrefilterCB
{
    float roughness = 0.0f;
    glm::vec3 _pad = {};
};

/// @brief UITransform定数バッファ
struct FALU_ENGINE_API UITransformCB
{
    glm::mat4 orthoProjection;  //-平行投影行列
    glm::vec2 position;         //-位置
    glm::vec2 size;             //-大きさ
    float rotation;             //-回転
    glm::vec3 _pad;             //-
};

/// @brief UIマテリアル定数バッファ
struct FALU_ENGINE_API UIMaterialCB
{
    glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };  //-色
    int useTexture = 0; //-テクスチャ使用フラグ
    glm::vec3 _pad;
};

class FALU_ENGINE_API DX11Renderer final : public IRenderer {
public:
    /// @brief コピー防止コンストラクタ
    /// @param  
    DX11Renderer(const DX11Renderer&) = delete;
    DX11Renderer& operator=(const DX11Renderer&) = delete;

    DX11Renderer()  = default;
    ~DX11Renderer() override { shutdown(); }
    /// @brief 初期化
    /// @param windowHandle 
    /// @param width 横幅
    /// @param height 縦幅
    /// @return 初期化成功フラグ
    bool init(void* windowHandle, uint32_t width, uint32_t height) override;
    /// @brief 終了処理
    void shutdown() override;
    /// @brief 現在のフレームを開始
    void beginFrame() override;
    /// @brief 現在のフレームを終了
    void endFrame()   override;

    /// @brief ライトの更新
    /// @param lightData 全てのライトデータ
    void updateLights(const LightCB& lightData);

    /// @brief 現在のシーンを描画
    /// @param scene 現在のシーン
    void renderScene(const Scene& scene) override;
    /// @brief ウィンドウの大きさを変更する
    /// @param width 新しい横幅
    /// @param height 新しい縦幅
    void onResize(uint32_t width, uint32_t height) override;

    /// @brief アクティブのカメラ位置を取得する
    /// @return 現在アクティブのカメラの位置
    glm::vec3 getCameraPosition() const override;

    [[nodiscard]] uint32_t getWidth()  const noexcept override { return m_width; }
    [[nodiscard]] uint32_t getHeight() const noexcept override { return m_height; }
    [[nodiscard]] uint32_t getActiveWidth() const noexcept { return m_activeWidth; }
    [[nodiscard]] uint32_t getActiveHeight() const noexcept { return m_activeHeight; }

    // DirectX オブジェクトへの直接アクセス（他サブシステムから使う場合）
    [[nodiscard]] ID3D11Device*        getDevice()  const noexcept { return m_device.Get(); }
    [[nodiscard]] ID3D11DeviceContext* getContext() const noexcept { return m_context.Get(); }

    /// @brief 使用しているGPUの名前を取得
    /// @return 使用しているGPUの名称
    [[nodiscard]] const std::string& getAdapterName() const noexcept { return m_adapterName; }

    /// @brief PBR描画パスの開始時に一度だけ呼ぶ。
    void bindPBRFrameResources();

    [[nodiscard]] const glm::mat4& getView() const noexcept { return m_view; }
    [[nodiscard]] const glm::mat4& getProjection() const noexcept { return m_projection; }

    [[nodiscard]] ID3D11Buffer* getBoundVB() const noexcept { return m_boundVB; }
    [[nodiscard]] ID3D11Buffer* getBoundIB() const noexcept { return m_boundIB; }
    void setBoundVB(ID3D11Buffer* vb)noexcept { m_boundVB = vb; }
    void setBoundIB(ID3D11Buffer* ib)noexcept { m_boundIB = ib; }
    [[nodiscard]] ID3D11VertexShader* getBoundVS() const noexcept { return m_boundVS; }
    [[nodiscard]] ID3D11PixelShader* getBoundPS() const noexcept { return m_boundPS; }

    //======== draw ==============

    /// @brief メッシュ描画
    /// @param vertices 頂点
    /// @param vertexCount 頂点数
    /// @param indices 頂点番号
    /// @param indexCount 頂点番号数
    /// @param transform 
    void drawMesh(const Vertex* vertices, uint32_t vertexCount,
        const uint32_t* indices, uint32_t indexCount,
        const glm::mat4& transform
    );
    /// @brief サブメッシュ描画
    /// @param indexOffset 頂点番号開始位置調整
    /// @param indexCount 頂点番号数
    /// @param transform 
    /// @param material 
    void drawSubMeshPBR(uint32_t indexOffset, uint32_t indexCount,
        const glm::mat4& transform,
        struct MaterialAsset* material);

    /// @brief 空描画
    /// @param view ビュー行列
    /// @param proj プロジェクション行列
    /// @param settings スカイ設定
    /// @param srv シェーダーリソースビュー
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
    [[nodiscard]] const RenderTexture* getGameRenderTexture() const noexcept
    {
        return m_gameRT.get();
    }

    void setViewProjection(const glm::mat4& view, const glm::mat4& projection) {
        m_view = view;
        m_projection = projection;
    }

    void bindShadowVertexStage(bool skinned);
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
        const glm::mat4& transform, struct MaterialAsset* material);

    // UI
    void drawUIQuad(const glm::vec2& position, const glm::vec2& size, float rotation,
        const glm::vec4& color, ID3D11ShaderResourceView* srv = nullptr);
    void beginUIPass(uint32_t screenWidth, uint32_t screenHeight);
    void endUIPass();
    void setUILocalMousePos(const glm::vec2& pos) noexcept { m_uiLocalMousePos = pos; }
    [[nodiscard]] glm::vec2 getUILocalMousePos() const noexcept { return m_uiLocalMousePos; }

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
    void uploadMaterialCB(const MaterialCB& mat);
    void uploadTransformCB(const glm::mat4& world);
    void unbindPBRFrameResources();

    bool createRenderTargetView();
    bool createDepthStencilView();
    bool createShaders(const std::filesystem::path& vsPath,const std::filesystem::path& psPath);
    bool createDefaultStates();

    void updateViewport();

    void restoreMainRenderTarget();
    void drawGrid(const glm::mat4& view, const glm::mat4& proj, float gridSize = 50.0f);

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

    // Cubeメッシュ用バッファ
    ComPtr<ID3D11Buffer> m_cubemapVB;
    ComPtr<ID3D11Buffer> m_cubemapIB;

    // SkinMesh
    ComPtr<ID3D11VertexShader> m_skinnedVertexShader;
    ComPtr<ID3D11VertexShader> m_shadowSkinnedVS;
    ComPtr<ID3D11InputLayout> m_skinnedInputLayout;
    ComPtr<ID3D11InputLayout> m_shadowSkinnedInputLayout;
    ComPtr<ID3D11Buffer> m_skinningCB;

    // UI
    ComPtr<ID3D11VertexShader> m_uiVertexShader;
    ComPtr<ID3D11PixelShader> m_uiPixelShader;
    ComPtr<ID3D11InputLayout> m_uiInputLayout;
    ComPtr<ID3D11Buffer> m_uiTransformCB;
    ComPtr<ID3D11Buffer> m_uiMaterialCB;
    ComPtr<ID3D11Buffer> m_uiQuadVB;
    ComPtr<ID3D11Buffer> m_uiQuadIB;
    ComPtr<ID3D11BlendState> m_uiBlendState;
    ComPtr<ID3D11DepthStencilState> m_uiDepthState;
    ComPtr<ID3D11RasterizerState> m_uiRasterizerState;
    glm::mat4 m_uiOrthoProjection = glm::mat4(1.0f);
    glm::vec2 m_uiLocalMousePos = { 0.0f,0.0f };
    

    ID3D11Buffer* m_boundVB = nullptr;
    ID3D11Buffer* m_boundIB = nullptr;
    ID3D11VertexShader* m_boundVS = nullptr;
    ID3D11PixelShader* m_boundPS = nullptr;
    ID3D11InputLayout* m_boundLayout = nullptr;

    //====== PBR描画の定数バッファ更新キャッシュ(内容が同じならMap/Unmapを省く) 
    struct TransformKey { glm::mat4 world; glm::mat4 view; glm::mat4 proj; }; // Transformのキャッシュに含む情報
    TransformKey m_lastTransformKey = {};   //-前回のキャッシュ(Transform)
    bool m_transformCBValid = false;       //-
    MaterialCB m_lastMaterialCB = {};       //-一前回のキャッシュ(Material)
    bool m_materialCBValid = false;
    glm::mat4 m_lightVP = glm::mat4(1.0f);
    std::string m_adapterName;
    std::unique_ptr<RenderTexture> m_sceneRT;
    std::unique_ptr<RenderTexture> m_gameRT;
    bool m_offscreen = false;
    bool m_gameOffscreen = false;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    uint32_t m_activeWidth = 0;
    uint32_t m_activeHeight = 0;
    bool     m_vsync  = true;

    float m_clearColor[4] = { 0.18f, 0.18f, 0.20f, 1.0f };

    glm::mat4 m_view = glm::mat4(1.0f);
    glm::mat4 m_projection = glm::mat4(1.0f);
    glm::vec3 m_cameraPos = glm::vec3(0.0f);
};

} // namespace FaluEngine
