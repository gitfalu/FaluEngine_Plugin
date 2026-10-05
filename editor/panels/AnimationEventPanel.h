#pragma once 
#include <imgui.h>
#include <entt/entt.hpp>
#include <string>

namespace FaluEngine { class Scene; }

namespace Editor
{
	class AnimationEventPanel
	{
	public:
		void draw(FaluEngine::Scene* scene, entt::entity selected);

	private:
		enum class Drag { None, Move, ResizeLeft, ResizeRight, Point };

		int m_selectedWindow = -1;
		int m_selectedPoint = -1;
		float m_pixelPerFrame = 8.0f;
		float m_dragOrigStart = 0.0f;
		float m_dragOrigEnd = 0.0f;
		float m_dragOrigFrame = 0.0f;
		int m_defaultLengthFrames = 6;
		char m_newPointName[64] = "SE_Event";
		std::string m_lastSaveError;
	};
}