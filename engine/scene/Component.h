#pragma once
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <memory>
#include <functional>
#include "Camera.h"
#include "asset/loaders/MeshLoader.h"
#include "script/ScriptInstance.h"
#include "scripting/NativeScript.h"
#include "asset/loaders/TextureLoader.h"
#include "renderer/dx11/DX11Renderer.h"
#include "asset/loaders/ShaderLoader.h"
#include "asset/loaders/MaterialLoader.h"
#include "asset/loaders/AnimationClip.h"
#include "ui/UITypes.h"
#include "audio/AudioClip.h"
#include "audio/AudioEngine.h"

namespace FaluEngine {

//=== 親子関係 =========================================
struct RelationshipComponent {
    entt::entity parent = entt::null;
    std::vector<entt::entity> children;
};


// ── 名前タグ ──────────────────────────────────────────────────────────────
struct TagComponent {
    std::string name;
    std::string category = "Untagged";
    explicit TagComponent(std::string n = "Entity") : name(std::move(n)) {}
};

// ── トランスフォーム ───────────────────────────────────────────────────────
struct TransformComponent {
    glm::vec3 position = { 0.f, 0.f, 0.f };
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 rotationEulerHint{ 0.0f };
    glm::vec3 scale    = { 1.f, 1.f, 1.f };

    [[nodiscard]] glm::mat4 getMatrix() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    glm::mat4 worldMatrix = glm::mat4(1.0f);

    // 便利メソッド
    void setRotationEuler(const glm::vec3& eularDeg) {
        rotationEulerHint = eularDeg;
        rotation = glm::quat(glm::radians(eularDeg));
    }

    void setRotationQuat(const glm::quat& q)
    {
        rotation = q;
        rotationEulerHint = glm::degrees(glm::eulerAngles(q));
    }
};

// ── メッシュ参照 ───────────────────────────────────────────────────────────
struct MeshComponent {
    std::string meshPath;   // AssetManager に渡すパス
    std::string materialPath;
    bool visible = true;

    std::shared_ptr<MeshAsset> cachedMesh;
    std::shared_ptr<MaterialAsset> cachedMaterial;
};

// ── カメラ ────────────────────────────────────────────────────────────────
struct CameraComponent {
    Camera camera;
    bool isPrimary = false;

    CameraComponent() {
        camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }
};

// ── Lua スクリプト ────────────────────────────────────────────────────────
struct ScriptComponent {
    std::string scriptPath; // assets/scripts/xxx.lua
    std::unique_ptr<ScriptInstance> instance;

    ~ScriptComponent()
    {
        instance.reset();
    }
};

struct NativeScriptComponent
{
    std::unique_ptr<NativeScript> instance;
    // 実生成遅延用
    std::function<std::unique_ptr<NativeScript>()> factory;
    bool initialize = false;

    template<typename T>
    void bind()
    {
        factory = []() { return std::make_unique<T>(); };
    }
};

struct LightComponent
{
    FaluEngine::LightType type = FaluEngine::LightType::Directional;
    glm::vec3 color = { 1.0f,1.0f,1.0f };
    float intensity = 1.0f;
    float range = 10.0f;
    float spotInner = 20.0f;
    float spotOuter = 30.0f;
    bool enable = true;

    // Shadow settings
    bool castShadow = true;
    bool softShadow = true;
    float shadowBias = 0.005f;
    float pcfRadius = 1.5f;
};

struct SkySphereComponent
{
    std::string texturePath;
    glm::vec4 topColor = { 0.1f,0.3f,0.7f,1.0f };
    glm::vec4 horizonColor = { 0.6f,0.7f,0.8f,1.0f };
    glm::vec4 bottomColor = { 0.2f,0.15f,0.1f,1.0f };
    float exposure = 1.0f;
    bool enabled = true;
    bool environmentBaked = false;

    std::shared_ptr<FaluEngine::TextureAsset> cachedTexture;
};

struct AnimatorComponent
{
    std::string currentClipName;
    float playbackTime = 0.0f;
    float playbackSpeed = 1.0f;
    bool playing = true;
    bool loop = true;

    std::vector<glm::mat4> boneMatrices;
};

struct ImageComponent
{
    std::string texturePath;
    glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
    bool visible = true;

    std::shared_ptr<TextureAsset> cachedTexture;
};

struct ButtonComponent
{
    bool interactable = true;
    glm::vec4 normalColor = { 1.0f,1.0f,1.0f,1.0f };
    glm::vec4 hoveredColor = { 0.9f,0.9f,0.9f,1.0f };
    glm::vec4 pressedColor = { 0.7f,0.7f,0.7f,1.0f };
    bool isHovered = false;
    bool isPressed = false;
};

struct TextComponent
{
    std::string text;
    float fontSize = 24.0f;
    glm::vec4 color = { 1,1,1,1 };
    std::string fontPath;
};

//======= Audio ========

struct AudioSourceComponent
{
    std::string clipPath;
    float volume = 1.0f;
    bool loop = false;
    bool playOnAwake = true;

    // 内部状態(エディタには描画しない)
    bool hasStarted = false;
    AudioVoiceHandle handle;
};

} // namespace FaluEngine
