#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace FaluEngine
{
	struct Bone {
		std::string name;
		int parentIndex = -1;
		glm::mat4 offsetMatrix = glm::mat4(1.0f);
		glm::mat4 localBindTransform = glm::mat4(1.0f);
	};

	struct Skeleton {
		std::vector<Bone> bones;
		std::unordered_map<std::string, int>boneNameToIndex;

		[[nodiscard]] int findBoneIndex(const std::string& name) const {
			auto it = boneNameToIndex.find(name);
			return it != boneNameToIndex.end() ? it->second : -1;
		}
	};

	static constexpr int MAX_BONE_INFLUENCE = 4;

	struct BoneWeightData {
		int boneIndices[MAX_BONE_INFLUENCE] = { -1,-1,-1,-1 };
		float boneWeights[MAX_BONE_INFLUENCE] = { 0.0f,0.0f,0.0f,0.0f };

		void addBonedata(int boneIndex, float weight)
		{
			for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
			{
				if (boneWeights[i] == 0.0f)
				{
					boneIndices[i] = boneIndex;
					boneWeights[i] = weight;
					return;
				}
			}

			int minIdx = 0;
			for (int i = 1; i < MAX_BONE_INFLUENCE; ++i)
			{
				if (boneWeights[i] < boneWeights[minIdx])minIdx = i;
			}
			if (weight > boneWeights[minIdx])
			{
				boneIndices[minIdx] = boneIndex;
				boneWeights[minIdx] = weight;
			}
		}

		void normalize()
		{
			float sum =
				boneWeights[0] + boneWeights[1] +
				boneWeights[2] + boneWeights[3];
			if (sum > 0.0001f)
			{
				for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
				{
					boneWeights[i] /= sum;
				}
			}
				
		}
	};
}
