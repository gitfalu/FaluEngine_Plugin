#include "AnimationClip.h"
#include "core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <algorithm>

namespace FaluEngine
{
	static glm::vec3 aiVec3ToGlm(const aiVector3D& v) { return { v.x, v.y, v.z }; }
	static glm::quat aiQuatToGlm(const aiQuaternion& q) { return { q.w, q.x, q.y,q.z }; }

	glm::mat4 BoneAnimChannel::sample(float time) const
	{
		glm::vec3 pos = interpolatePosition(time);
		glm::quat rot = interpolateRotation(time);
		glm::vec3 scale = interpolateScale(time);

		glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
		glm::mat4 r = glm::mat4_cast(rot);
		glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
		return t * r * s;
	}

	glm::vec3 BoneAnimChannel::interpolatePosition(float time) const
	{
		if (positionKeys.empty()) return glm::vec3(0.0f);
		if (positionKeys.size() == 1) return positionKeys[0].value;

		for (size_t i = 0; i + 1 < positionKeys.size(); ++i)
		{
			if (time < positionKeys[i + 1].time)
			{
				float t0 = positionKeys[i].time;
				float t1 = positionKeys[i + 1].time;
				float factor = (time - t0) / std::max(t1 - t0, 0.0001f);
				factor = glm::clamp(factor, 0.0f, 1.0f);
				return glm::mix(positionKeys[i].value, positionKeys[i + 1].value, factor);
			}
		}
		return positionKeys.back().value;
	}

	glm::quat BoneAnimChannel::interpolateRotation(float time) const
	{
		if (rotationKeys.empty()) return glm::identity<glm::quat>();
		if (rotationKeys.size() == 1) return rotationKeys[0].value;

		for (size_t i = 0; i + 1 < rotationKeys.size(); ++i)
		{
			if (time < rotationKeys[i + 1].time)
			{
				float t0 = rotationKeys[i].time;
				float t1 = rotationKeys[i + 1].time;
				float factor = (time - t0) / std::max(t1 - t0, 0.0001f);
				factor = glm::clamp(factor, 0.0f, 1.0f);
				return glm::slerp(rotationKeys[i].value, rotationKeys[i + 1].value, factor);
			}
		}
		return rotationKeys.back().value;
	}

	glm::vec3 BoneAnimChannel::interpolateScale(float time) const
	{
		if (scaleKeys.empty()) return glm::vec3(1.0f);
		if (scaleKeys.size() == 1) return scaleKeys[0].value;

		for (size_t i = 0; i + 1 < scaleKeys.size(); ++i)
		{
			if (time < scaleKeys[i + 1].time)
			{
				float t0 = scaleKeys[i].time;
				float t1 = scaleKeys[i + 1].time;
				float factor = (time - t0) / std::max(t1 - t0, 0.0001f);
				factor = glm::clamp(factor, 0.0f, 1.0f);
				return glm::mix(scaleKeys[i].value, scaleKeys[i + 1].value, factor);
			}
		}
		return scaleKeys.back().value;
	}

	std::vector<std::shared_ptr<AnimationClip>>
		loadAnimationFromFile(const std::string& meshPath)
	{
		std::vector<std::shared_ptr<AnimationClip>> clips;

		if (!std::filesystem::exists(meshPath))
		{
			LOG_ERROR("AnimationClip: file not found '{}'", meshPath);
			return clips;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(meshPath,
			aiProcess_Triangulate | aiProcess_LimitBoneWeights);

		if (!scene || !scene->HasAnimations()) {
			LOG_WARN("AnimationClip: no animation found in '{}'", meshPath);
			return clips;
		}

		for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
		{
			const aiAnimation* anim = scene->mAnimations[a];

			auto clip = std::make_shared<AnimationClip>();
			clip->name = anim->mName.length > 0 ? anim->mName.C_Str() : ("Anim" + std::to_string(a));
			clip->ticksPerSecond = 
				(anim->mTicksPerSecond != 0.0f)
				? static_cast<float>(anim->mTicksPerSecond) 
				: 25.0f;
			clip->duration = static_cast<float>(anim->mDuration) / clip->ticksPerSecond;

			for (uint32_t c = 0; c < anim->mNumChannels; ++c)
			{
				const aiNodeAnim* nodeAnim = anim->mChannels[c];

				BoneAnimChannel channel;
				channel.boneName = nodeAnim->mNodeName.C_Str();

				for (uint32_t k = 0; k < nodeAnim->mNumPositionKeys; ++k)
				{
					float t = static_cast<float>(nodeAnim->mPositionKeys[k].mTime) / clip->ticksPerSecond;
					channel.positionKeys.push_back({ t,aiVec3ToGlm(nodeAnim->mPositionKeys[k].mValue) });
				}
				for (uint32_t k = 0; k < nodeAnim->mNumRotationKeys; ++k)
				{
					float t = static_cast<float>(nodeAnim->mRotationKeys[k].mTime) / clip->ticksPerSecond;
					channel.rotationKeys.push_back({ t,aiQuatToGlm(nodeAnim->mRotationKeys[k].mValue) });
				}
				for (uint32_t k = 0; k < nodeAnim->mNumScalingKeys; ++k)
				{
					float t = static_cast<float>(nodeAnim->mScalingKeys[k].mTime) / clip->ticksPerSecond;
					channel.scaleKeys.push_back({ t,aiVec3ToGlm(nodeAnim->mScalingKeys[k].mValue) });
				}

				clip->boneNameToChannel[channel.boneName] = clip->channels.size();
				clip->channels.push_back(std::move(channel));
			}

			clip->valid = true;
			clips.push_back(clip);

			LOG_INFO("AnimationClip: loaded '{}' from '{}' (duration = {:.2f}s,{} channels)",
				clip->name, meshPath, clip->duration, clip->channels.size());
		}
		return clips;
	}

	void registerAnimationLoader()
	{

	}
}
