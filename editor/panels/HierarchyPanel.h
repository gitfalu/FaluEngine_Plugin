#pragma once 
#include <imgui.h>
#include <entt/entt.hpp>

namespace FaluEngine
{
	class Scene;
	class Entity;
}


namespace Editor
{
	class HierarchyPanel
	{
	public:
		void draw(FaluEngine::Scene* scene);

		[[nodiscard]] entt::entity getSelected() const noexcept { return m_selected; }
		void setSelected(entt::entity e) noexcept { m_selected = e; }
		void clearSelected() noexcept { m_selected = entt::null; }
	private:
		void drawEntityNode(FaluEngine::Scene* scene, entt::entity entity);

		entt::entity m_selected = entt::null;
	};
}
