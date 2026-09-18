#include "FaluEngine/ReplayRecorder.h"

#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"

#include <algorithm>

namespace FaluEngine
{
	ReplayRecorder::ReplayRecorder(size_t maxFrames) : m_capacity(maxFrames) {}

	void ReplayRecorder::reset()
	{
		m_baseSnapshot.clear();
		m_lastValues.clear();
		m_frames.clear();
	}

	void ReplayRecorder::setCapacity(size_t maxFrames)
	{
		m_capacity = maxFrames;
		while (m_frames.size() > m_capacity)
		{
			foldFrameInto(m_baseSnapshot, m_frames.front());
			m_frames.pop_front();
		}
	}

	uint64_t ReplayRecorder::oldestFrame() const noexcept
	{
		return m_frames.empty() ? 0 : m_frames.front().frameNo;
	}

	uint64_t ReplayRecorder::latestFrame() const noexcept
	{
		return m_frames.empty() ? 0 : m_frames.back().frameNo;
	}

	
	void ReplayRecorder::captureFrame(Scene& scene, uint64_t frameNo)
	{
		FrameRecord record;
		record.frameNo = frameNo;

		std::unordered_map<uint64_t, bool> seen;

		scene.each<IDComponent>(
			[&](Entity e, IDComponent& id)
			{
				seen[id.uuid] = true;

				if (scene.registry().all_of<TransformComponent>(e))
				{
					auto& t = scene.registry().get<TransformComponent>(e);
					TransformSnapshotRaw snap{ t.position,t.rotation,t.scale };

					auto it = m_lastValues.find(id.uuid);
					const bool isNew = (it == m_lastValues.end()) || !it->second.hasTransform;
					if (isNew)
					{
						m_baseSnapshot[id.uuid].hasTransform = true;
						m_baseSnapshot[id.uuid].transform = snap;
						m_lastValues[id.uuid].hasTransform = true;
						m_lastValues[id.uuid].transform = snap;
					}
					else
					{
						auto& prev = it->second.transform;
						const bool changed =
							prev.position != snap.position ||
							prev.rotation != snap.rotation ||
							prev.scale != snap.scale;
						if (changed)
						{
							record.transChanged.push_back({ id.uuid,snap });
							it->second.transform = snap;
						}
					}
				}

				if (scene.registry().all_of<AnimatorComponent>(e))
				{
					auto& a = scene.registry().get<AnimatorComponent>(e);
					AnimatorSnapshotRaw snap{ a.playbackTime,a.playing };

					auto it = m_lastValues.find(id.uuid);
					const bool isNew = (it == m_lastValues.end()) || !it->second.hasAnimator;
					if (isNew)
					{
						m_baseSnapshot[id.uuid].hasAnimator = true;
						m_baseSnapshot[id.uuid].animator = snap;
						m_lastValues[id.uuid].hasAnimator = true;
						m_lastValues[id.uuid].animator = snap;
					}
					else
					{
						auto& prev = it->second.animator;
						const bool changed =
							prev.playbackTime != snap.playbackTime ||
							prev.playing != snap.playing;
						if (changed)
						{
							record.animChanged.push_back({ id.uuid,snap });
							it->second.animator = snap;
						}
					}
				}
			});

		// ‘OƒtƒŒ[ƒ€‚Æ”äŠr‚µŒ©‚Â‚©‚ç‚È‚­‚È‚Á‚½uuid‚ð”jŠü‚·‚é
		for (auto& [uuid, state] : m_lastValues)
		{
			if (seen.find(uuid) == seen.end())
			{
				record.destroyedUuids.push_back(uuid);
			}
		}
		// “o˜^‚µ‚½íœƒŠƒXƒg‚ð‚à‚Æ‚É•ÛŽƒŠƒXƒg‚©‚çíœ
		for (auto uuid : record.destroyedUuids)
		{
			m_lastValues.erase(uuid);
		}

		m_frames.push_back(std::move(record));

		if (m_frames.size() > m_capacity)
		{
			foldFrameInto(m_baseSnapshot,m_frames.front());
			m_frames.pop_front();
		}
	}
	
	void ReplayRecorder::seekTo(Scene& scene, uint64_t frameNo)
	{
		if (m_frames.empty()) return;

		frameNo = std::clamp(frameNo, oldestFrame(), latestFrame());

		auto state = m_baseSnapshot;
		for (auto& frame: m_frames)
		{
			if (frame.frameNo > frameNo) break;
			foldFrameInto(state, frame);
		}
		applyState(scene, state);
	}

	void ReplayRecorder::commitSeek(Scene& scene, uint64_t frameNo)
	{
		if (m_frames.empty()) return;
		frameNo = std::clamp(frameNo, oldestFrame(), latestFrame());

		auto state = m_baseSnapshot;
		while (!m_frames.empty() && m_frames.front().frameNo <= frameNo)
		{
			foldFrameInto(state, m_frames.front());
			m_frames.pop_front();
		}

		m_frames.clear();

		m_baseSnapshot = state;
		m_lastValues = state;

		applyState(scene, state);
	}

	void ReplayRecorder::foldFrameInto(std::unordered_map<uint64_t, EntityState>& state, const FrameRecord& frame) const
	{
		for (auto& d : frame.transChanged)
		{
			auto& s = state[d.uuid];
			s.hasTransform = true;
			s.transform = d.snapshot;
		}

		for (auto& d : frame.animChanged)
		{
			auto& s = state[d.uuid];
			s.hasAnimator = true;
			s.animator = d.snapshot;
		}

		for (auto uuid : frame.destroyedUuids)
		{
			state.erase(uuid);
		}
	}

	void ReplayRecorder::applyState(Scene& scene, const std::unordered_map<uint64_t, EntityState>& state) const
	{
		std::unordered_map<uint64_t, entt::entity> uuidToEntity;
		scene.each<IDComponent>([&](Entity e, IDComponent& id) {
			uuidToEntity[id.uuid] = e;
			});

		for (auto& [uuid, s] : state)
		{
			auto it = uuidToEntity.find(uuid);
			if (it == uuidToEntity.end()) continue;

			if (s.hasTransform && scene.registry().all_of<TransformComponent>(it->second))
			{
				auto& t = scene.registry().get<TransformComponent>(it->second);
				t.position = s.transform.position;
				t.rotation = s.transform.rotation;
				t.scale = s.transform.scale;
			}

			if (s.hasAnimator && scene.registry().all_of<AnimatorComponent>(it->second))
			{
				auto& a = scene.registry().get<AnimatorComponent>(it->second);
				a.playbackTime = s.animator.playbackTime;
				a.playing = s.animator.playing;
			}
		}
	}
}
