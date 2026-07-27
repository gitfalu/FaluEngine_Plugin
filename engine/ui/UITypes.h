#pragma once
#include <glm/glm.hpp>

namespace FaluEngine
{

	enum class CanvasRenderMode {
		ScreenSpaceOverlay,
		WorldSpace,
	};

	struct RectTransformComponent
	{
		glm::vec2 anchorMin = { 0.5f,0.5f };
		glm::vec2 anchorMax = { 0.5f,0.5f };
		glm::vec2 anchoredPos = { 0.0f,0.0f };
		glm::vec2 sizeDelta = { 100.0f,100.0f };
		glm::vec2 pivot = { 0.5f,0.5f };

		float rotation = 0.0f;
		glm::vec2 scale = { 1.0f,1.0f };

		glm::vec2 computedPosition = { 0.0f,0.0f };
		glm::vec2 computedSize = { 100.0f,100.0f };
	};

	struct CanvasComponent
	{
		CanvasRenderMode renderMode = CanvasRenderMode::ScreenSpaceOverlay;
		glm::vec2 referenceResolution = { 1920.0f,1080.0f };
		int sortOrder = 0;
		bool enabled = true;
	};

}
