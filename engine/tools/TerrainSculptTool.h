#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <d3d11.h>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Scene;
	class DX11Renderer;
	struct TerrainComponent;
}

namespace Editor
{
	enum class BrushMode
	{
		Raise,
		Lower,
		Flatten,
		Smooth,
	};

	class FALU_ENGINE_API TerrainSculptTool
	{
	public:
		static TerrainSculptTool& get();

		void setTarget(FaluEngine::Scene* scene, entt::entity entity);
		void clearTarget();
		[[nodiscard]] bool hasTarget() const noexcept { return m_scene != nullptr && m_entity != entt::null; }
		[[nodiscard]] entt::entity getTargetEntity() const noexcept { return m_entity; }

		bool enabled = false;
		BrushMode mode = BrushMode::Raise;
		float radius = 5.0f;
		float strength = 3.0f;

		void update(FaluEngine::DX11Renderer* renderer, ID3D11Device* device,
			float mouseX, float mouseY, float imageW, float imageH, bool mouseDown,
			bool invertModifierHeld, float deltaTime);

		[[nodiscard]] bool getLastHit(glm::vec3& outWorldPos) const
		{
			outWorldPos = m_lastHitWorld;
			return m_hasHit;
		}

	private:
		bool raycastHeightfield(
			FaluEngine::TerrainComponent& terrain,
			const glm::mat4& worldMatrix, const glm::vec3& rayOrigin,
			const glm::vec3& rayDir, float maxDistance,
			glm::vec3& outHitLocal
		) const;

		void applyBrush(FaluEngine::TerrainComponent& terrain, const glm::vec3& hitLocal,float dt,bool invert);

	private:
		FaluEngine::Scene* m_scene = nullptr;
		entt::entity m_entity = entt::null;

		bool m_hasHit = false;
		glm::vec3 m_lastHitWorld{ 0.0f };
		glm::vec3 m_lastHitLocal{ 0.0f };

		bool m_wasMouseDown = false;
		float m_flattenTargetHeight = 0.0f;
	};
}
