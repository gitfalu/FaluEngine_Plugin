#include <FaluEngine/SpriteAnimationSystem.h>
#include "FaluEngine/Component.h"
#include "FaluEngine/EventBus.h"
#include "asset/AssetManager.h"
#include "asset/loaders/SpriteAsset.h"
#include "asset/loaders/TextureLoader.h"
#include "core/Logger.h"

#include <algorithm>
#include <cmath>

namespace FaluEngine
{
	namespace // Helper
	{
		constexpr int kMaxLoopsPerFrame = 8; // 巨大なdtでイベントが無限に出るのを防ぐ

		bool resolveAssets(SpriteAnimatorComponent& sa)
		{
			if (sa.clipPath.empty())
			{
				sa.cachedClip.reset();
				sa.cachedSheet.reset();
				sa.cachedClipPath.clear();
				return false;
			}
			if (sa.cachedClip && sa.cachedClipPath == sa.clipPath && sa.cachedSheet)
				return true;

			sa.cachedClipPath = sa.clipPath;
			sa.cachedClip = AssetManager::get().load<SpriteClipAsset>(sa.clipPath);
			sa.cachedSheet.reset();
			if (!sa.cachedClip) return false;

			if (!sa.cachedClip->sheetPath.empty())
				sa.cachedSheet = AssetManager::get().load<SpriteSheetAsset>(sa.cachedClip->sheetPath);
			return sa.cachedSheet != nullptr;
		}

		void writeFrameToImage(SpriteAnimatorComponent& sa, ImageComponent& img)
		{
			const auto sample = sampleSpriteClip(*sa.cachedClip, sa.time);
			auto& sheet = *sa.cachedSheet;

			// スプライトアニメーションの画像ファイルと現在のテクスチャパスが違う場合置き換える
			if (img.texturePath != sheet.texturePath)
			{
				img.texturePath = sheet.texturePath;
				img.cachedTexture.reset();
			}

			// シートにサイズが書かれていなければ、ロード済みテクスチャから補う
			if ((sheet.textureSize.x <= 0 || sheet.textureSize.y <= 0) &&
				img.cachedTexture && img.cachedTexture->width > 0)
			{
				sheet.textureSize = { static_cast<int>(img.cachedTexture->width),
										static_cast<int>(img.cachedTexture->height) };
			}

			img.uvRect = sheet.uvRectOf(sample.slice);
			if (sa.pixelPerfect) img.pointFilter = true;
			sa.lastFrame = sample.frame;
		}

		void publishEvents(entt::registry& registry, entt::entity entity,
			const SpriteClipAsset& clip, float from, float to, bool includeFrom)
		{
			for (const auto& ev : clip.events)
			{
				const bool hit = includeFrom ? (ev.time >= from && ev.time <= to)
					: (ev.time > from && ev.time <= to);
				if (!hit) continue;

				SpriteAnimationEvent e;
				e.entity = static_cast<uint32_t>(entity);
				if (auto* id = registry.try_get<IDComponent>(entity)) e.entityUUID = id->uuid;
				e.name = ev.name;
				e.payload = ev.payload;
				EventBus::get().publish(e);
			}
		}
	}

	void updateSpriteAnimations(entt::registry& registry, float deltaTime)
	{
		auto view = registry.view<SpriteAnimatorComponent, ImageComponent>();
		for (auto entity : view)
		{
			auto& sa = view.get<SpriteAnimatorComponent>(entity);
			auto& img = view.get<ImageComponent>(entity);

			if (!resolveAssets(sa)) continue;
			const SpriteClipAsset& clip = *sa.cachedClip;
			const float total = clip.totalDuration();

			if (sa.playing && total > 0.0f)
			{
				const float prev = sa.time;
				float cur = prev + deltaTime * sa.speed;

				if (clip.loop)
				{
					const int prevLoop = static_cast<int>(std::floor(prev / total));
					const int curLoop = (std::min)(static_cast<int>(std::floor(cur / total)),
						prevLoop + kMaxLoopsPerFrame);
					for (int l = prevLoop; l <= curLoop; ++l)
					{
						const float start = (l == prevLoop) ? std::fmod(prev, total) : 0.0f;
						const float end = (l == curLoop) ? std::fmod(cur, total) : total;
						const float first = (l == prevLoop) && !sa.started;
						publishEvents(registry, entity, clip, start, end, first);
					}
				}
				else
				{
					cur = (std::min)(cur, total);
					publishEvents(registry, entity, clip, prev, cur, !sa.started);
					if (cur >= total) sa.playing = false;
				}
				sa.time = cur;
				sa.started = true;
			}
			writeFrameToImage(sa, img);
		}
	}

	void applySprteAnimatorFrame(entt::registry& registry, entt::entity entity)
	{
		auto* sa = registry.try_get<SpriteAnimatorComponent>(entity);
		auto* img = registry.try_get<ImageComponent>(entity);
		if (!sa || !img) return;
		if (!resolveAssets(*sa)) return;
		writeFrameToImage(*sa, *img);
	}
}
