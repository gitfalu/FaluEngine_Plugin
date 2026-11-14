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
				size = (anchorMaxPx - anchorMinPx) + rt.sizeDelta;
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

	/*
	* 
	* エディタ編集
	* 
	*/
	
	ParentFrame parentFrameOf(entt::registry& registry, entt::entity entity)
	{
		ParentFrame pf;
		auto* rel = registry.try_get<RelationshipComponent>(entity);
		if (!rel || rel->parent == entt::null || !registry.valid(rel->parent)) return pf;

		if (auto* prt = registry.try_get<RectTransformComponent>(rel->parent))
		{// 親の情報を取得して適用する
			pf.pos = prt->computedPosition;
			pf.size = prt->computedSize;
			pf.rotation = prt->computedRotation;
		}
		else if (auto* canvas = registry.try_get<CanvasComponent>(rel->parent))
		{
			pf.size = canvas->computedSize;
		}

		return pf;
	}

	void relayout(entt::registry& registry, entt::entity entity)
	{
		const ParentFrame pf = parentFrameOf(registry, entity);
		computeRect(registry, entity, pf.pos, pf.size, pf.rotation);
		
	}

	void setWorldRect(entt::registry& registry, entt::entity entity, 
		const glm::vec2& center, const glm::vec2& scaledSize, 
		float totalRotationDeg)
	{
		auto* rtp = registry.try_get<RectTransformComponent>(entity);
		if (!rtp) return;
		auto& rt = *rtp;
		const ParentFrame pf = parentFrameOf(registry, entity);

		rt.rotation = totalRotationDeg - pf.rotation;

		const glm::vec2 sc = {
			std::fabs(rt.scale.x) < 1e-4f ? 1e-4f : rt.scale.x,
			std::fabs(rt.scale.y) < 1e-4f ? 1e-4f : rt.scale.y
		};
		const glm::vec2 size = scaledSize / sc;

		const glm::vec2 parentMin = pf.pos - pf.size * 0.5f;
		const glm::vec2 aMin = parentMin + pf.size * rt.anchorMin;
		const glm::vec2 aMax = parentMin + pf.size * rt.anchorMax;

		glm::vec2 base;
		if (rt.anchorMin == rt.anchorMax)
		{
			rt.sizeDelta = size;
			base = aMin;
		}
		else
		{
			rt.sizeDelta = size - (aMax - aMin);
			base = (aMin + aMax) * 0.5f;
		}

		// 中心 = pivot + R(total) * scaleSize*(0.5f - pivot) を逆に解く
		const glm::vec2 pivotWorld = center - rotate(scaledSize * (glm::vec2(0.5f, 0.5f) - rt.pivot), totalRotationDeg);
		const glm::vec2 pivotPoint = pf.pos + rotate(pivotWorld - pf.pos, -pf.rotation);
		rt.anchoredPos = pivotPoint - base;

		relayout(registry, entity);
	}

	void applyAnchorPress(entt::registry& registry, entt::entity entity, HAlign h, VAlign v)
	{
		auto* rtp = registry.try_get<RectTransformComponent>(entity);
		if (!rtp) return;
		auto& rt = *rtp;

		relayout(registry, entity); // 現在の見た目を確定させてから変更を行う
		const glm::vec2 center = rt.computedPosition;
		const glm::vec2 size = rt.computedSize;
		const float rot = rt.computedRotation;

		switch (h)
		{
		case HAlign::Left: 
			rt.anchorMin.x = rt.anchorMax.x = 0.0f; rt.pivot.x = 0.0f;
			break;
		case HAlign::Center: 
			rt.anchorMin.x = rt.anchorMax.x = 0.5f; rt.pivot.x = 0.5f;
			break;
		case HAlign::Right: 
			rt.anchorMin.x = rt.anchorMax.x = 1.0f; rt.pivot.x = 1.0f;
			break;
		case HAlign::Stretch:
			rt.anchorMin.x = 0.0f; rt.anchorMax.x = 1.0f; rt.pivot.x = 0.5f;
			break;
		}
		switch (v)
		{
		case VAlign::Top:
			rt.anchorMin.y = rt.anchorMax.y = 1.0f; rt.pivot.y = 1.0f;
			break;
		case VAlign::Middle:
			rt.anchorMin.y = rt.anchorMax.y = 0.5f; rt.pivot.y = 0.5f;
			break;
		case VAlign::Bottom:
			rt.anchorMin.y = rt.anchorMax.y = 0.0f; rt.pivot.y = 0.0f;
			break;
		case VAlign::Stretch:
			rt.anchorMin.y = 0.0f; rt.anchorMax.y = 1.0f; rt.pivot.y = 0.5f;
			break;
		}

		setWorldRect(registry, entity, center, size, rot);
	}

	RectGeom resizeRect(const RectGeom& orig, HandleDir h, const glm::vec2& deltaWorld, bool symmetric, bool keepAspect, float minSize)
	{
		const glm::vec2 d = rotate(deltaWorld, -orig.rotation); // 矩形のローカル軸へ
		const glm::vec2 half = orig.size * 0.5f;
		glm::vec2 lo = -half, hi = half;

		if (h.sx > 0) { hi.x += d.x; if (symmetric) lo.x -= d.x; }
		if (h.sx < 0) { lo.x += d.x; if (symmetric) hi.x -= d.x; }
		if (h.sy > 0) { hi.y += d.y; if (symmetric) lo.y -= d.y; }
		if (h.sy < 0) { lo.y += d.y; if (symmetric) hi.y -= d.y; }

		if (keepAspect && h.sx != 0 && h.sy != 0 && orig.size.x > 0.0f && orig.size.y > 0.0f)
		{
			const float sx = (hi.x - lo.x) / orig.size.x;
			const float sy = (hi.y - lo.y) / orig.size.y;
			const float s = (std::max)(sx, sy);
			const glm::vec2 ns = orig.size * s;
			if (symmetric)
			{
				lo = -ns * 0.5f; hi = ns * 0.5f;
			}
			else
			{
				const glm::vec2 fixedCorner = { -static_cast<float>(h.sx) * half.x,-static_cast<float>(h.sy) * half.y };
				if (h.sx > 0)	{ lo.x = fixedCorner.x; hi.x = fixedCorner.x + ns.x; }
				else			{ hi.x = fixedCorner.x; lo.x = fixedCorner.x - ns.x; }
				if (h.sy > 0)	{ lo.y = fixedCorner.y; hi.y = fixedCorner.y + ns.y; }
				else			{ hi.y = fixedCorner.y; lo.y = fixedCorner.y - ns.y; }
			}
		}

		if (hi.x - lo.x < minSize)
		{
			if (symmetric) { lo.x = -minSize * 0.5f; hi.x = minSize * 0.5f; }
			else if (h.sx > 0) hi.x = lo.x + minSize; else lo.x = hi.x - minSize;
		}
		if (hi.y - lo.y < minSize)
		{
			if (symmetric) { lo.y = -minSize * 0.5f; hi.y = minSize * 0.5f; }
			else if (h.sy > 0) hi.y = lo.y + minSize; else lo.y = hi.y - minSize;
		}

		return RectGeom();
	}

	SnapResult computeSnapOffset(const glm::vec2& rectMin, const glm::vec2& size, const glm::vec2& parentMin, const glm::vec2& parentSize, float gridSize, float threshold)
	{
		return SnapResult();
	}

}