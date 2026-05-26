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
    LOG_TRACE("Entity created: '{}'", name);
    return e;
}

void Scene::destroyEntity(Entity entity) {
    if (entity.hasComponent<TagComponent>()) {
        LOG_TRACE("Entity destroyed: '{}'", entity.getComponent<TagComponent>().name);
    }
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

void Scene::onRender() {

    auto* renderer = static_cast<DX11Renderer*>(
        Application::getInstance().getRenderer());
    if (!renderer) return;

    auto view = m_registry.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);
        if (!cam.isPrimary) continue;

        float aspect = 
            static_cast<float>(renderer->getWidth()) /
            static_cast<float>(renderer->getHeight());
        if (std::abs(aspect - cam.camera.getAspectRatio()) > 0.001f) {
            cam.camera.setPerspective(
                cam.camera.getFovDeg(), aspect,
                cam.camera.getNearClip(), cam.camera.getFarClip());
        }

        renderer->setViewProjection(
            cam.camera.getView(), cam.camera.getProjection()
        );

        break;
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
            if (mesh.cachedTexture && mesh.cachedTexture->srv) {
                renderer->drawSubMeshTextured(
                    sub.indexOffset, sub.indexCount,
                    transform.getMatrix(),
                    mesh.cachedTexture->srv.Get()
                );
            }
            else
            {
                renderer->drawSubMesh(sub.indexOffset, sub.indexCount, transform.getMatrix());
            }
        }
    }
}

} // namespace FaluEngine
