#include "Entity.h"
#include "Scene.h"
#include "Component.h"
#include <algorithm>

namespace FaluEngine {

entt::registry& Entity::getRegistry() {
    return m_scene->registry();
}

const entt::registry& Entity::getRegistry() const {
    return m_scene->registry();
}

void Entity::setParent(Entity parent)
{
    if (!isValid() || !parent.isValid()) return;

    // 既存の親からの切り離し
    removeParent();

    auto& rel = getRegistry().get_or_emplace<RelationshipComponent>(m_handle);
    rel.parent = static_cast<entt::entity>(parent);

    auto& parentRel = getRegistry().get_or_emplace<RelationshipComponent>(
        static_cast<entt::entity>(parent));
    parentRel.children.push_back(m_handle);

    if (m_scene) m_scene->removeFromRootOrder(*this);
}

void Entity::removeParent()
{ 
    if (!isValid())return;
    if (!hasComponent<RelationshipComponent>()) return;

    auto& rel = getComponent<RelationshipComponent>();
    if (rel.parent == entt::null) return;

    // 親のchildrenリストから自分を削除
    auto& parentRel = getRegistry().get<RelationshipComponent>(rel.parent);
    parentRel.children.erase(
        std::remove(parentRel.children.begin(),
            parentRel.children.end(), m_handle),
        parentRel.children.end());

    rel.parent = entt::null;

    if (m_scene) m_scene->addToRootOrder(*this);
}

void Entity::addChild(Entity child)
{
    if (child.isValid()) child.setParent(*this);
}

void Entity::removeChild(Entity child)
{
    if (child.isValid()) child.removeParent();
}

Entity Entity::getParent() const
{
    if (!hasComponent<RelationshipComponent>()) return{};
    auto& rel = getComponent<RelationshipComponent>();
    return Entity(rel.parent, m_scene);
}

std::vector<Entity> Entity::getChildren() const
{
    if (!hasComponent<RelationshipComponent>()) return {};
    auto& rel = getComponent<RelationshipComponent>();
    std::vector<Entity> result;
    result.reserve(rel.children.size());
    for (auto child : rel.children)
        result.emplace_back(child, m_scene);
    return result;
}

bool Entity::hasParent() const
{
    if (!hasComponent<RelationshipComponent>()) return false;
    return getComponent<RelationshipComponent>().parent != entt::null;
}

bool Entity::hasChildren() const
{
    if (!hasComponent<RelationshipComponent>()) return false;
    return !getComponent<RelationshipComponent>().children.empty();
}

} // namespace FaluEngine
