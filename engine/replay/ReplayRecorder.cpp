#include "FaluEngine/ReplayRecorder.h"

#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include "physics/RigidbodyComponent.h"
#include "physics/PhysicsSystem.h"
#include "FaluEngine/ActionEventSystem.h"

#include <algorithm>

namespace FaluEngine
{
	namespace
	{
		JPH::Vec3 toJPH(const glm::vec3& v) { return { v.x,v.y,v.z }; }
		JPH::Quat toJPH(const glm::quat& q) { return { q.x,q.y,q.z,q.w }; }
		glm::vec3 toGLM(const JPH::Vec3& v) { return { v.GetX(),v.GetY(),v.GetZ() }; }
	}

	ReplayRecorder::ReplayRecorder(size_t maxFrames) : m_capacity(maxFrames) {}

	void ReplayRecorder::reset()
	{
		m_baseSnapshot.clear();
		m_lastValues.clear();
		m_frames.clear();
	}

	std::string ReplayRecorder::describeFrameRange(Scene& scene, uint64_t fromFrame, uint64_t toFrame) const
	{
		if (fromFrame > toFrame) std::swap(fromFrame, toFrame);

		std::unordered_map<uint64_t, std::string> nameOf;
		scene.each<IDComponent>([&](Entity e, IDComponent& id) {
			std::string name = "uuid:" + std::to_string(id.uuid);
			if (scene.registry().all_of<TagComponent>(e))
			{
				name = scene.registry().get<TagComponent>(e).name + "(uuid:" + std::to_string(id.uuid) + ")";
			}
			nameOf[id.uuid] = name;
			});

		auto resolveName = [&](uint64_t uuid) -> std::string {
			auto it = nameOf.find(uuid);
			return it != nameOf.end() ? it->second : ("uuid:" + std::to_string(uuid) + "(現在は破棄済み)");
			};

		std::string out;
		out.reserve(2048);
		out += "=== Replay frames " + std::to_string(fromFrame) + " - " + std::to_string(toFrame) + " ===\n";

		bool any = false;
		for (auto& frame : m_frames)
		{
			if (frame.frameNo < fromFrame || frame.frameNo > toFrame) continue;
			any = true;

			out += "--- frame " + std::to_string(frame.frameNo) + " ---\n";

			for (auto& d : frame.transChanged)
			{
				const auto& p = d.snapshot.position;
				out += " [Transform] " + resolveName(d.uuid) +
					" pos=(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + ")\n";
			}

			for (auto& d : frame.animChanged)
			{
				out += " [Animator] " + resolveName(d.uuid) +
					" time=" + std::to_string(d.snapshot.playbackTime) +
					" playing=" + (d.snapshot.playing ? "true" : "false") + "\n";
			}

			for (auto& d : frame.rigidChanged)
			{
				const auto& lv = d.snapshot.linearVelocity;
				const auto& av = d.snapshot.angularVelocity;
				out += " [Rigidbody] " + resolveName(d.uuid) +
					" linVel=(" + std::to_string(lv.x) + ", " + std::to_string(lv.y) + ", " + std::to_string(lv.z) + ")" +
					" angVel=(" + std::to_string(av.x) + ", " + std::to_string(av.y) + ", " + std::to_string(av.z) + ")\n";
			}

			for (auto uuid : frame.destroyedUuids)
			{
				out += " [Destroyed] " + resolveName(uuid) + "\n";
			}
		}

		if(!any)
		{
			out += "(この区間に記録済みフレームがありません。バッファ範囲外の可能性があります)\n";
		}

		return out;
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

				if (m_lastValues.find(id.uuid) == m_lastValues.end())
				{
					m_baseSnapshot[id.uuid];
					m_lastValues[id.uuid];
				}

				// 各コンポーネントずつ値に変更のあったオブジェクトをスナップショットして保管する
				if (scene.registry().all_of<TransformComponent>(e))
				{
					auto& t = scene.registry().get<TransformComponent>(e);
					TransformSnapshotRaw snap{ t.position,t.rotation,t.scale };

					auto it = m_lastValues.find(id.uuid);
					const bool isNew = (it == m_lastValues.end()) || !it->second.hasTransform;
					if (isNew)
					{// UUID
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

				if (scene.registry().all_of<RigidbodyComponent>(e))
				{
					auto& rb = scene.registry().get<RigidbodyComponent>(e);

					if (rb.registered && !rb.bodyID.IsInvalid())
					{
						auto& bodyInterface = PhysicsSystem::get().getBodyInterface();
						RigidbodySnapshotRaw snap{
							toGLM(bodyInterface.GetLinearVelocity(rb.bodyID)),
							toGLM(bodyInterface.GetAngularVelocity(rb.bodyID))
						};

						auto it = m_lastValues.find(id.uuid);
						const bool isNew = (it == m_lastValues.end()) || !it->second.hasRigidbody;
						if (isNew)
						{
							m_baseSnapshot[id.uuid].hasRigidbody = true;
							m_baseSnapshot[id.uuid].rigidbody = snap;
							m_lastValues[id.uuid].hasRigidbody = true;
							m_lastValues[id.uuid].rigidbody = snap;
						}
						else
						{
							auto& prev = it->second.rigidbody;
							const bool changed =
								prev.linearVelocity != snap.linearVelocity ||
								prev.angularVelocity != snap.angularVelocity;

							if (changed)
							{
								record.rigidChanged.push_back({ id.uuid,snap });
								it->second.rigidbody = snap;
							}
						}
					}
				}
			});

		// 前フレームと比較し見つからなくなったuuidを破棄する
		for (auto& [uuid, state] : m_lastValues)
		{
			if (seen.find(uuid) == seen.end())
			{
				record.destroyedUuids.push_back(uuid);
			}
		}
		// 登録した削除リストをもとに保持リストから削除
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

		for (auto& d : frame.rigidChanged)
		{
			auto& s = state[d.uuid];
			s.hasRigidbody = true;
			s.rigidbody = d.snapshot;
		}

		for (auto uuid : frame.destroyedUuids)
		{
			state.erase(uuid);
		}
	}

	void ReplayRecorder::applyState(Scene& scene, const std::unordered_map<uint64_t, EntityState>& state) const
	{
		std::unordered_map<uint64_t, FaluEngine::Entity> uuidToEntity;
		scene.each<IDComponent>([&](Entity e, IDComponent& id) {
			uuidToEntity[id.uuid] = e;
			});

		for (auto& [uuid, e] : uuidToEntity)
		{
			if (state.find(uuid) == state.end())
			{
				scene.destroyEntity(e);
			}
		}

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

				if (scene.registry().all_of<RigidbodyComponent>(it->second))
				{
					auto& rb = scene.registry().get<RigidbodyComponent>(it->second);

					if (rb.registered && !rb.bodyID.IsInvalid())
					{
						auto& bodyInterface = PhysicsSystem::get().getBodyInterface();
						bodyInterface.SetPositionAndRotation(
							rb.bodyID,
							toJPH(t.position),
							toJPH(t.rotation),
							JPH::EActivation::Activate
						);

						const glm::vec3 linVel = s.hasRigidbody ? s.rigidbody.linearVelocity : glm::vec3(0.0f);
						const glm::vec3 angVel = s.hasRigidbody ? s.rigidbody.angularVelocity : glm::vec3(0.0f);

						bodyInterface.SetLinearVelocity(rb.bodyID, toJPH(linVel));
						bodyInterface.SetAngularVelocity(rb.bodyID, toJPH(angVel));
					}
				}
			}

			if (s.hasAnimator && scene.registry().all_of<AnimatorComponent>(it->second))
			{
				auto& a = scene.registry().get<AnimatorComponent>(it->second);
				a.playbackTime = s.animator.playbackTime;
				a.playing = s.animator.playing;
			}
		}

		ActionEventSystem::resyncAll(scene.registry());
	}
}
