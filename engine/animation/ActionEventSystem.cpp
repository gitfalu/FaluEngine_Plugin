#include <FaluEngine/ActionEventSystem.h>

#include <FaluEngine/ActionEventLibrary.h>
#include <FaluEngine/Component.h>
#include <FaluEngine/EventBus.h>

#include "asset/loaders/AnimationCache.h"

#include <cmath>

namespace FaluEngine
{
	namespace
	{
		bool refreshTrack(ActionStateComponent& s, const std::string& meshPath, const std::string& clipName)
		{
			const bool clipChanged = { s.cachedMeshPath != meshPath || s.cachedClipName != clipName };

			if (clipChanged || s.cachedRevision != ActionEventLibrary::get().revision())
			{
				s.track = ActionEventLibrary::get().find(meshPath, clipName);
				s.cachedMeshPath = meshPath;
				s.cachedClipName = clipName;
				s.cachedRevision = ActionEventLibrary::get().revision();
			}

			return clipChanged;
		}

		uint64_t uuidOf(entt::registry& reg, entt::entity e)
		{
			if (auto* id = reg.try_get<IDComponent>(e)) return id->uuid;
			return 0;
		}
	}

	void ActionEventSystem::tick(entt::registry& reg, entt::entity e,
		const AnimatorComponent& animator, const std::string& meshPath,
		float clipDuration, float prevTime, bool wrapped, float deltaTime)
	{
		auto* existing = reg.try_get<ActionStateComponent>(e);

		if (!existing)
		{
			if (!ActionEventLibrary::get().find(meshPath, animator.currentClipName)) return;
			existing = &reg.emplace<ActionStateComponent>(e);
		}
		ActionStateComponent& s = *existing;

		const bool clipChanged = refreshTrack(s, meshPath, animator.currentClipName);

		ActionStepInput in;
		in.prevTime = prevTime;
		in.curTime = animator.playbackTime;
		in.duration = clipDuration;
		in.wrapped = wrapped;
		in.clipChanged = clipChanged && s.hasEvaluated;

		const float expected = deltaTime * std::fabs(animator.playbackSpeed);
		if (!wrapped && !in.clipChanged && std::fabs(in.curTime - in.prevTime) > expected * 1.5f + 0.001f)
			in.seek = true;

		ActionTrackEvaluator::step(s, s.track.get(), in);
		publish(reg, e, s, in.curTime);
	}

	void ActionEventSystem::resync(entt::registry& reg, entt::entity e, const AnimatorComponent& animator,
		const std::string& meshPath, float clipDuration)
	{
		auto* s = reg.try_get<ActionStateComponent>(e);
		if (!s)
		{
			if (!ActionEventLibrary::get().find(meshPath, animator.currentClipName)) return;
			s = &reg.emplace<ActionStateComponent>(e);
		}
		refreshTrack(*s, meshPath, animator.currentClipName);

		ActionStepInput in;
		in.prevTime = animator.playbackTime;
		in.curTime = animator.playbackTime;
		in.duration = clipDuration;
		in.seek = true;
		ActionTrackEvaluator::step(*s, s->track.get(),in);
	}

	void ActionEventSystem::resyncAll(entt::registry& reg)
	{
		auto view = reg.view<AnimatorComponent,MeshComponent>();
		for (auto e : view)
		{
			auto& animator = view.get<AnimatorComponent>(e);
			auto& mesh = view.get<MeshComponent>(e);
			auto clip = AnimationCache::get().getClip(mesh.meshPath, animator.currentClipName);
			if (!clip) continue;
			resync(reg, e, animator, mesh.meshPath, clip->duration);
		}
	}

	void ActionEventSystem::publish(entt::registry& reg, entt::entity e, const ActionStateComponent& s, float timeSeconds)
	{
		if (!s.enteredMask && !s.exitedMask && s.firedPoints.empty()) return;

		auto& bus = EventBus::get();
		const uint64_t uuid = uuidOf(reg, e);

		for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
		{
			const auto type = static_cast<ActionWindowType>(i);
			if (s.exitedMask & actionWindowBit(type))
				bus.publish(ActionWindowEvent{ e, uuid,type,false,timeSeconds });
		}
		for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
		{
			const auto type = static_cast<ActionWindowType>(i);
			if (s.exitedMask & actionWindowBit(type))
				bus.publish(ActionWindowEvent{ e, uuid,type,true,timeSeconds });
		}
		for (const auto& p : s.firedPoints)
			bus.publish(ActionPointFiredEvent{ e,uuid,p.name,p.payload,p.timeSeconds });
	}
}
