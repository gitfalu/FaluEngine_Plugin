#pragma once
#include <FaluEngine/EngineExport.h>
#include <glm/glm.hpp>

namespace FaluEngine
{

	enum class FALU_GRAPHICS_API CanvasRenderMode {
		ScreenSpaceOverlay,
		WorldSpace,
	};

	/// @brief Canvasのスケール方式
	enum class FALU_GRAPHICS_API CanvasScaleMode
	{
		ConstatPixelSize, // 実ピクセルでそのままレイアウト
		ScaleWithScreenSize, // referenceResolutionを基準に画面サイズへ拡縮
	};

	struct FALU_GRAPHICS_API RectTransformComponent
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
		float computedRotation = 0.0f; // 親の回転を含む累計回転
	};

	struct FALU_GRAPHICS_API CanvasComponent
	{
		CanvasRenderMode renderMode = CanvasRenderMode::ScreenSpaceOverlay;
		glm::vec2 referenceResolution = { 1920.0f,1080.0f };
		int sortOrder = 0;
		bool enabled = true;

		CanvasScaleMode scaleMode = CanvasScaleMode::ConstatPixelSize;
		float matchWidthOrHeight = 0.5f; // 0:幅基準 1:高さ基準

		// ランタイム値
		glm::vec2 computedSize = { 0.0f,0.0f }; // レイアウト空間のサイズ
		float scaleFactor = 1.0f; // レイアウト空間->実ピクセル
	};

}
