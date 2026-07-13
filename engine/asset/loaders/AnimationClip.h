#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "asset/AssetManager.h"

namespace FaluEngine
{
	//===== Key Frame ======
	struct PositionKey { float time; glm::vec3 value; };
	struct RotationKey { float time; glm::quat value; };
	struct ScaleKey { float time; glm::vec3 value; };

	/// @brief 1ボーン分のアニメーションチャンネル
	struct BoneAnimChannel
	{
		std::string boneName;
		std::vector<PositionKey> positionKeys;
		std::vector<RotationKey> rotationKeys;
		std::vector<ScaleKey> scaleKeys;

		[[nodiscard]] glm::mat4 sample(float time) const;

	private:
		glm::vec3 interpolatePosition(float time) const;
		glm::quat interpolateRotation(float time) const;
		glm::vec3 interpolateScale(float time) const;
	};

	//========= Animation Clip ==========
	/// @brief 1つのアニメーション
	struct AnimationClip : public Asset
	{
		std::string name;
		float duration = 0.0f;
		float ticksPerSecond = 25.0f;
		bool loop = true;

		std::vector<BoneAnimChannel> channels;
		std::unordered_map<std::string, size_t> boneNameToChannel;

		[[nodiscard]] const BoneAnimChannel* findChannel(const std::string& boneName) const
		{
			auto it = boneNameToChannel.find(boneName);
			return it != boneNameToChannel.end() ? &channels[it->second] : nullptr;
		}

		bool valid = false;
	};

	std::vector<std::shared_ptr<AnimationClip>> loadAnimationFromFile(const std::string& meshPath);

	void registerAnimationLoader();
}
