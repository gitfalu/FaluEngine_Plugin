#include "Scene.h"
#include "Entity.h"
#include "Component.h"
#include "core/Logger.h"
#include "core/Application.h"
#include "asset/AssetManager.h"
#include "asset/loaders/MeshLoader.h"
#include "renderer/dx11/DX11Renderer.h"
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"
#include "physics/PhysicsSystem.h"

namespace FaluEngine {

Scene::Scene(std::string name) : m_name(std::move(name)) {
    LOG_INFO("Scene created: '{}'", m_name);
}
Scene::~Scene() {
    LOG_INFO("Scene destroyed: '{}'", m_name);
}

Entity Scene::createEntity(const std::string& name) {
    Entity e(m_registry.create(), this);
    e.addComponent<TagComponent>(name);
    e.addComponent<TransformComponent>();
    e.addComponent<RelationshipComponent>();
    LOG_TRACE("Entity created: '{}'", name);
    return e;
}

void Scene::destroyEntity(Entity entity) {
    if (!entity.isValid()) return;

    if (m_registry.all_of<RelationshipComponent>(entity))
    {
        auto& rel = m_registry.get<RelationshipComponent>(entity);

        auto children = rel.children;
        for (auto child : children)
        {
            Entity childEntity(child, this);
            destroyEntity(childEntity);
        }

        if (rel.parent != entt::null)
        {
            auto& parentRel = m_registry.get<RelationshipComponent>(rel.parent);
            parentRel.children.erase(
                std::remove(parentRel.children.begin(),
                    parentRel.children.end(),
                    static_cast<entt::entity>(entity)),
                parentRel.children.end());
        }
    }

    if (entity.hasComponent<TagComponent>())
        LOG_TRACE("Entity destroyed: '{}'",
            entity.getComponent<TagComponent>().name);

    m_registry.destroy(entity);
}

void Scene::onUpdate(float deltaTime) {
    PhysicsSystem::get().step(deltaTime);
    PhysicsSystem::get().syncTransforms(*this);

    // luaスクリプトによる更新処理
    auto scriptView = m_registry.view<ScriptComponent>();
    for (auto entity : scriptView) {
        auto& sc = scriptView.get<ScriptComponent>(entity);
        if (sc.scriptPath.empty()) continue;

        Entity e(entity, this);

        if (!sc.instance) {
            sc.instance = std::make_unique<ScriptInstance>(
                ScriptEngine::get().getLua(), sc.scriptPath);
            sc.instance->onInit(e);
        }

        if (sc.instance && sc.instance->isValid()) {
            sc.instance->onUpdate(e,deltaTime);
        }
    }
}

static void computeWorldMatrix(entt::registry& registry, entt::entity entity,
    const glm::mat4& parentWorld)
{
    auto& transform = registry.get<TransformComponent>(entity);
    transform.worldMatrix = parentWorld * transform.getMatrix();

    if (registry.all_of<RelationshipComponent>(entity))
    {
        auto& rel = registry.get<RelationshipComponent>(entity);
        for (auto child : rel.children)
            computeWorldMatrix(registry, child, transform.worldMatrix);
    }
}

void Scene::onRender() {
    auto* renderer = static_cast<DX11Renderer*>(
        Application::getInstance().getRenderer());
    if (!renderer) return;

    auto relView = m_registry.view<RelationshipComponent, TransformComponent>();
    for (auto entity : relView)
    {
        auto& rel = relView.get<RelationshipComponent>(entity);
        if (rel.parent == entt::null)
        {
            auto& t = relView.get<TransformComponent>(entity);
            t.worldMatrix = t.getMatrix();
            for (auto child : rel.children)
                computeWorldMatrix(m_registry, child, t.worldMatrix);
        }
    }

    //====== ライト情報を収集して更新 ======
    {
        LightCB lightCB;
        lightCB.lightCount = 0;

        // カメラを更新しつつカメラ位置を取得
        auto camView = m_registry.view<CameraComponent>();
        for (auto entity : camView)
        {
            auto& cam = camView.get<CameraComponent>(entity);
            if (!cam.isPrimary) continue;

            float aspect = 
                static_cast<float>(renderer->getWidth()) /
                static_cast<float>(renderer->getHeight());
            if (std::abs(aspect - cam.camera.getAspectRatio()) > 0.001f) {
                cam.camera.setPerspective(
                    cam.camera.getFovDeg(), aspect,
                    cam.camera.getNearClip(), cam.camera.getFarClip());
            }
            
            // Shadow Pass
            {
                auto lightView = m_registry.view<LightComponent, TransformComponent>();
                for (auto entity : lightView)
                {
                    auto& lc = lightView.get<LightComponent>(entity);
                    auto& lt = lightView.get<TransformComponent>(entity);
                    if (!lc.enable || lc.type != LightType::Directional) continue;
                    if (!lc.castShadow) continue;

                    glm::vec3 lightDir = glm::normalize(
                        lt.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
                    glm::vec3 lightPos = -lightDir * 20.0f;

                    glm::mat4 lView = glm::lookAtLH(
                        lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::mat4 lProj = glm::orthoLH(
                        -20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);

                    renderer->beginShadowPass(lView, lProj);

                    auto meshView = m_registry.view<MeshComponent, TransformComponent>();
                    for (auto meshEntity : meshView)
                    {
                        auto& mesh = meshView.get<MeshComponent>(meshEntity);
                        auto& transform = meshView.get<TransformComponent>(meshEntity);
                        if (!mesh.visible || !mesh.cachedMesh) continue;

                        auto* vb = mesh.cachedMesh->vertexBuffer.Get();
                        auto* ib = mesh.cachedMesh->indexBuffer.Get();
                        if (renderer->getBoundVB() != vb)
                        {
                            UINT stride = sizeof(Vertex), offset = 0;
                            renderer->getContext()->IASetVertexBuffers(
                                0, 1, &vb, &stride, &offset);
                            renderer->setBoundVB(vb);
                        }
                        if (renderer->getBoundIB() != ib)
                        {
                            UINT stride = sizeof(Vertex), offset = 0;
                            renderer->getContext()->IASetIndexBuffer(
                                ib, DXGI_FORMAT_R32_UINT, 0);
                            renderer->setBoundIB(ib);
                        }
                        for (const auto& sub : mesh.cachedMesh->subMeshes)
                            renderer->drawShadowMesh(sub.indexOffset, sub.indexCount,
                                transform.getMatrix());

                    }
                    renderer->endShadowPass();

                    ShadowSettingsCB shadowSettings;
                    shadowSettings.lightSpaceMatrix = glm::transpose(lProj * lView);
                    shadowSettings.useShadow = 1;
                    shadowSettings.useSoftShadow = lc.softShadow ? 1 : 0;
                    shadowSettings.shadowBias = lc.shadowBias;
                    shadowSettings.pcfRadius = lc.pcfRadius;
                    renderer->updateShadowSettings(shadowSettings);

                    break;
                }
            }
            
            renderer->setViewProjection(
                cam.camera.getView(), cam.camera.getProjection()
            );

            if (cam.isPrimary) {
                lightCB.cameraPos = cam.camera.getPosition();
            }

            break;
        }

        auto lightView = m_registry.view<LightComponent, TransformComponent>();
        for (auto entity : lightView)
        {
            if (lightCB.lightCount >= 16) break;
            auto& lc = lightView.get<LightComponent>(entity);
            auto& lt = lightView.get<TransformComponent>(entity);
            if (!lc.enable)continue;

            LightData& ld = lightCB.lights[lightCB.lightCount++];
            ld.position = { lt.position.x,lt.position.y,lt.position.z ,1.0f};
            ld.color = { lc.color.r,lc.color.g ,lc.color.b,lc.intensity };
            ld.type = static_cast<int>(lc.type);
            ld.range = lc.range;
            ld.spotInner = glm::radians(lc.spotInner);
            ld.spotOuter = glm::radians(lc.spotOuter);

            glm::vec3 forward = (lc.type == LightType::Directional) ?
                glm::normalize(lt.rotation * glm::vec3(0.0f, 0.0f, 1.0f)) :
                glm::normalize(lt.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
                

            ld.direction = { forward.x,forward.y,forward.z ,0.0f};
                
        }
        
        renderer->updateLights(lightCB);
    }

    // MeshComponent を持つエンティティをレンダラーへ送る
    auto meshView = m_registry.view<MeshComponent, TransformComponent>();
    for(auto entity : meshView)
    {
        auto& mesh = meshView.get<MeshComponent>(entity);
        auto& transform = meshView.get<TransformComponent>(entity);
        if (!mesh.visible || mesh.meshPath.empty()) continue;
        if (!mesh.cachedMesh)
            mesh.cachedMesh = AssetManager::get().load<MeshAsset>(mesh.meshPath);
        if (!mesh.cachedMesh || !mesh.cachedMesh->vertexBuffer || !mesh.cachedMesh->indexBuffer) continue;

        if (!mesh.texturePath.empty() && !mesh.cachedTexture)
            mesh.cachedTexture = AssetManager::get().load<TextureAsset>(mesh.texturePath);

        auto* vb = mesh.cachedMesh->vertexBuffer.Get();
        auto* ib = mesh.cachedMesh->indexBuffer.Get();
        if (renderer->getBoundVB() != vb) {
            UINT stride = sizeof(Vertex), offset = 0;
            renderer->getContext()->IASetVertexBuffers(
                0, 1, &vb, &stride, &offset);
            renderer->setBoundVB(vb);
        }

        if (renderer->getBoundIB() != ib)
        {
            renderer->getContext()->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
            renderer->setBoundIB(ib);
        }

        for (const auto& sub : mesh.cachedMesh->subMeshes) {
            ShaderAsset* customShader = nullptr;
            if (!mesh.vertexShaderPath.empty() && !mesh.pixelShaderPath.empty())
            {
                if (!mesh.cachedShader)
                {
                    std::string key = mesh.vertexShaderPath + "|" + mesh.pixelShaderPath;
                    mesh.cachedShader = AssetManager::get().load<ShaderAsset>(key);
                }
                if (mesh.cachedShader && mesh.cachedShader->valid)
                    customShader = mesh.cachedShader.get();
            }
            
            if (mesh.cachedTexture && mesh.cachedTexture->srv) {
                // ノーマルマップのキャッシュ
                if (!mesh.normalMapPath.empty() && !mesh.cachedNormalMap)
                    mesh.cachedNormalMap = AssetManager::get()
                    .load<TextureAsset>(mesh.normalMapPath);

                renderer->drawSubMeshTextured(
                    sub.indexOffset, sub.indexCount,
                    transform.worldMatrix,
                    mesh.cachedTexture->srv.Get(),
                    mesh.cachedNormalMap ? mesh.cachedNormalMap->srv.Get() : nullptr,
                    customShader
                );
            }
            else
            {
                renderer->drawSubMesh(
                    sub.indexOffset, sub.indexCount, 
                    transform.worldMatrix,
                    customShader);
            }
        }
    }

    {
        auto skyView = m_registry.view<SkySphereComponent>();
        for (auto entity : skyView)
        {
            auto& sky = skyView.get<SkySphereComponent>(entity);
            if (!sky.enabled) continue;

            if (!sky.texturePath.empty() && !sky.cachedTexture)
                sky.cachedTexture = AssetManager::get()
                .load<TextureAsset>(sky.texturePath);

            SkySettingsCB settings;
            settings.topColor = sky.topColor;
            settings.bottomColor = sky.bottomColor;
            settings.horizonColor = sky.horizonColor;
            settings.useTexture = (!sky.texturePath.empty() &&
                sky.cachedTexture) ? 1 : 0;
            settings.exposure = sky.exposure;

            renderer->drawSkySphere(
                renderer->getView(), renderer->getProjection(),
                settings,
                settings.useTexture ? sky.cachedTexture->srv.Get() : nullptr );
            break;
        }
    }
}

} // namespace FaluEngine
