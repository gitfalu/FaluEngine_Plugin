#include "UILayout.h"
#include "FaluEngine/Component.h"

#include <algorithm>
#include <cmath>

namespace FaluEngine::UILayout
{
	namespace
	{
		constexpr int kMaxDepth = 64; // 親子ループ等での無限再帰ガード

		glm::vec2 rotate(const glm::vec2& v, float deg)
		{
			if (deg == 0.0f) return v;
			const float r = glm::radians(deg);
			const float c = std::cos(r), s = std::sin(r);
			return { v.x * c - v.y * s,v.x + v.y * c };
		}

		void computeRectImpl(entt::registry& registry, entt::entity entity,
			const glm::vec2& parentPos, const glm::vec2& parentSize,
			float parentRot, int depth)
		{
			if (depth > kMaxDepth) return;
			auto* rtPtr = registry.try_get<RectTransformComponent>(entity);
			if (!rtPtr) return;
			auto& rt = *rtPtr;

			// 親の「回転していない座標系」でのアンカー基準の配置
			const glm::vec2 parentMin = parentPos - parentSize * 0.5f;
			const glm::vec2 anchorMinPx = parentMin + parentSize * rt.anchorMin;
			const glm::vec2 anchorMaxPx = parentMin + parentSize * rt.anchorMax;

			glm::vec2 size;
			glm::vec2 pivotPoint; // pivotがおかれる点
			if (rt.anchorMin == rt.anchorMax)
			{
				size = rt.sizeDelta;
				pivotPoint = anchorMinPx + rt.anchoredPos;
			}
			else
			{
				size = (anchorMaxPx - anchorMinPx) * rt.sizeDelta;
				pivotPoint = (anchorMinPx + anchorMaxPx) * 0.5f + rt.anchoredPos;
			}
			const glm::vec2 scaledSize = size * rt.scale;

			// 親の回転を、親の中心まわりでPivot位置に適用
			const glm::vec2 pivotWorld = parentPos + rotate(pivotPoint - parentPos, parentRot);

			// 自身の回転はPivotまわり。中心はPivotから(回転した)オフセット分ずらす
			const float totalRot = parentRot + rt.rotation;
			const glm::vec2 centerFromPivot = scaledSize * (glm::vec2(0.5f, 0.5f) - rt.pivot);

			rt.computedPosition = pivotWorld + rotate(centerFromPivot, totalRot);
			rt.computedSize = scaledSize;
			rt.computedRotation = totalRot;

			if (auto* rel = registry.try_get<RelationshipComponent>(entity))
			{
				const auto children = rel->children;
				for (auto child : children)
					computeRectImpl(registry, child, rt.computedPosition, rt.computedSize,
						rt.computedRotation, depth + 1);
			}
		}

		void collectImpl(entt::registry& registry, entt::entity entity,
			std::vector<entt::entity>& out, int depth)
		{
			if (depth > kMaxDepth) return;
			if(!registry.all_of<RectTransformComponent>(entity)) return;
			out.push_back(entity);
			if (auto* rel = registry.try_get<RelationshipComponent>(entity))
				for (auto child : rel->children)
					collectImpl(registry, child, out, depth + 1);
		}
	} // Helper


	CanvasMetrics computeCanvasMetrics(const CanvasComponent& canvas, const glm::vec2& screenSize)
	{
		CanvasMetrics m;
		m.size = screenSize;

		const glm::vec2& ref = canvas.referenceResolution;
		if (canvas.scaleMode != CanvasScaleMode::ScaleWithScreenSize ||
			ref.x <= 0.0f || ref.y <= 0.0f || screenSize.x <= 0.0f || screenSize.y <= 0.0f)
			return m;

		// UnityのCanvasScaler(ScaleWithScreenSize)と同じ
		const float match = std::clamp(canvas.matchWidthOrHeight, 0.0f, 1.0f);
		const float sx = screenSize.x / ref.x;
		const float sy = screenSize.y / ref.y;
		m.scale = std::pow(sx, 1.0f - match) * std::pow(sy, match);
		m.size = screenSize / m.scale;
		return m;
	}

	void computeRect(entt::registry& registry, entt::entity entity, 
		const glm::vec2& parentPos, const glm::vec2& parentSize, float parentRotationDeg)
	{
		computeRectImpl(registry, entity, parentPos, parentSize, parentRotationDeg, 0);
	}

	void layoutCanvas(entt::registry& registry, entt::entity canvasEntity, const glm::vec2& screenSize)
	{
		auto* canvas = registry.try_get<CanvasComponent>(canvasEntity);
		if (!canvas) return;

		const CanvasMetrics m = computeCanvasMetrics(*canvas, screenSize);
		canvas->computedSize = m.size;
		canvas->scaleFactor = m.scale;

		if (auto* rel = registry.try_get<RelationshipComponent>(canvasEntity))
		{
			const auto children = rel->children;
			for (auto child : children)
				computeRect(registry, child, { 0.0f,0.0f }, m.size, 0.0f);
		}
	}

	std::vector<entt::entity> collectCanvases(entt::registry& registry)
	{
		std::vector<entt::entity> out;
		for (auto e : registry.view<CanvasComponent>())
		{
			const auto& c = registry.get<CanvasComponent>(e);
			if (c.enabled && c.renderMode == CanvasRenderMode::ScreenSpaceOverlay)
				out.push_back(e);
		}
		std::stable_sort(out.begin(), out.end(), [&](entt::entity a, entt::entity b) {
			return registry.get<CanvasComponent>(a).sortOrder < registry.get<CanvasComponent>(b).sortOrder;
			});
		return out;
	}

	void collectDrawOrder(entt::registry& registry, entt::entity canvasEntity, std::vector<entt::entity>& out)
	{
		if (auto* rel = registry.try_get<RelationshipComponent>(canvasEntity))
			for (auto child : rel->children)
				collectImpl(registry, child, out, 0);
	}

	bool hitTest(const RectTransformComponent& rt, const glm::vec2& p)
	{
		const glm::vec2 d = p - rt.computedPosition;
		const float r = glm::radians(rt.computedRotation);
		const float c = std::cos(r), s = std::sin(r);
		const float lx = c * d.x + s * d.y;
		const float ly = -s * d.x + c * d.y;
		const glm::vec2 half = rt.computedSize * 0.5f;
		return std::fabs(lx) <= half.x && std::fabs(ly) <= half.y;
	}

}