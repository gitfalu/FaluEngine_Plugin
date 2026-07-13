#pragma once
#include "AnimationClip.h"
#include <unordered_map>
#include <mutex>

namespace FaluEngine
{
	class AnimationCache
	{
	public:
		static AnimationCache& get()
		{
			static AnimationCache instance;
			return instance;
		}

		const std::vector<std::shared_ptr<AnimationClip>>& getAnimations(const std::string& meshPath)
		{
			auto it = m_cache.find(meshPath);
			if (it != m_cache.end()) return it->second;

			auto clips = loadAnimationFromFile(meshPath);
			auto& stored = m_cache[meshPath];
			stored = std::move(clips);
			return stored;
		}

		std::shared_ptr<AnimationClip> getClip(const std::string& meshPath, const std::string& clipName)
		{
			auto& clips = getAnimations(meshPath);
			for (auto& clip : clips)
			{
				if (clip->name == clipName) return clip;
			}
			return nullptr;
		}

	private:
		AnimationCache() = default;
		std::unordered_map < std::string, std::vector<std::shared_ptr<AnimationClip>>> m_cache;
	};
}
