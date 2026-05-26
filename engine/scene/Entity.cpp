#include "Entity.h"
#include "Scene.h"

namespace FaluEngine {

entt::registry& Entity::getRegistry() {
    return m_scene->registry();
}

const entt::registry& Entity::getRegistry() const {
    return m_scene->registry();
}

} // namespace FaluEngine
