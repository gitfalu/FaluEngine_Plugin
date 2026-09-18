#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Scene;

	/// @brief 実行フレームを
	class FALU_ENGINE_API ReplayRecorder
	{
	public:
		explicit ReplayRecorder(size_t maxFrames = 600); // 60FPSで１０秒を目安にレコードを取る

		void setCapacity(size_t maxFrames);

		void captureFrame(Scene& scene, uint64_t frameNo);

		void seekTo(Scene& scene, uint64_t frameNo);

		void commitSeek(Scene& scene, uint64_t frameNo);

		void reset();

		[[nodiscard]] bool empty() const noexcept { return m_frames.empty(); }
		[[nodiscard]] uint64_t oldestFrame() const noexcept;
		[[nodiscard]] uint64_t latestFrame() const noexcept;

	private:
		struct TransformSnapshotRaw
		{
			glm::vec3 position{ 0.0f };
			glm::quat rotation = glm::identity<glm::quat>();
			glm::vec3 scale{ 1.0f };
		};

		struct AnimatorSnapshotRaw
		{
			float playbackTime = 0.0f;
			bool playing = true;
		};

		template<typename T>
		struct Diff
		{
			uint64_t uuid = 0;
			T snapshot{};
		};

		struct EntityState
		{
			bool hasTransform = false;
			TransformSnapshotRaw transform;
			bool hasAnimator = false;
			AnimatorSnapshotRaw animator;
		};

		struct FrameRecord
		{
			uint64_t frameNo = 0;
			std::vector<Diff<TransformSnapshotRaw>> transChanged;
			std::vector<Diff<AnimatorSnapshotRaw>> animChanged;
			std::vector<uint64_t> destroyedUuids;
		};

		void foldFrameInto(std::unordered_map<uint64_t, EntityState>& state, const FrameRecord& frame) const;
		void applyState(Scene& scene, const std::unordered_map<uint64_t, EntityState>& state) const;

		std::unordered_map<uint64_t, EntityState> m_baseSnapshot;
		std::unordered_map<uint64_t, EntityState> m_lastValues;
		std::deque<FrameRecord> m_frames;
		size_t m_capacity;

	};
}
