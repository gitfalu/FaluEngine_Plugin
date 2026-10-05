#include "AnimationEventPanel.h"

#include "FaluEngine/Scene.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Localization.h"
#include "FaluEngine/ActionEventTrack.h"
#include "FaluEngine/ActionEventLibrary.h"
#include "FaluEngine/ActionEventSystem.h"
#include "asset/loaders/AnimationCache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>


namespace Editor
{
	using namespace FaluEngine;

	namespace
	{
		constexpr float kRoeHieght = 22.0f;
		consteval float kRulerHeight = 22.0f;
		constexpr float LabelWidth = 96.0f;
		consteval float kHandleWidth = 6.0f;

		ImU32 colorOf(ActionWindowType t, float alpha = 1.0f)
		{
			static const ImVec4 c[] = {
				{0.95f, 0.75f, 0.20f, 1}, // Parry
				{0.30f, 0.65f, 0.95f, 1}, // JustGuard
				{0.35f, 0.85f, 0.55f, 1}, // JustDodge
				{0.75f, 0.75f, 0.80f, 1}, // Invincible
				{0.70f, 0.50f, 0.90f, 1}, // SuperArmor
				{0.95f, 0.35f, 0.35f, 1}, // HitActive
				{0.95f, 0.55f, 0.25f, 1}, // CancelWindow
				{0.40f, 0.80f, 0.85f, 1}, // InputBuffer
				{0.60f, 0.60f, 0.60f, 1}, // Custom
			};

			ImVec4 v = c{ static_cast<int>(t) };
			v.w = alpha;
			return ImGui::ColorConvertFloat4ToU32(v);
		}

		float defaultLengthFor(ActionWindowType t, int fallback)
		{
			switch (t)
			{
			case ActionWindowType::Parry: return 8.0f;
			case ActionWindowType::JustGuard: return 6.0f;
			case ActionWindowType::JustDodge: return 10.0f;
			case ActionWindowType::Invincible: return 20.0f;
			default: return static_cast<float>(fallback);
			}
		}
	}


	void AnimationEventPanel::draw(FaluEngine::Scene* scene, entt::entity selected)
	{
		ImGui::Begin(TR("Animation Events"));

		if(!scene || selected == entt::null || !scene->registry().valid(selected))
		{
			ImGui::Text(TR("No entity selected."));
			ImGui::End();
			return;
		}

		auto& reg = scene->registry();
		auto* animator = reg.try_get<AnimatorComponent>(selected);
		auto* mesh = reg.try_get<MeshComponent>(selected);
		if(!animator || !mesh || mesh->meshPath.empty())
		{
			ImGui::TextDisabled(TR("Selected entity has no Animator or Mesh component."));
			ImGui::End();
			return;
		}

		const auto& clips = AnimationCache::get().getAnimations(mesh->meshPath);
		if(clips.empty())
		{
			ImGui::TextDisabled(TR("No animation clips found for the mesh."));
			ImGui::End();
			return;
		}

		auto& lib = ActionEventLibrary::get();
		const std::string& meshPath = mesh->meshPath;


		//=========== Clip Selection ============
		{
			ImGui::SetNextItemWidth(260.0f);
			if (ImGui::BeginCombo(TR("Clip"), animator->currentClipName.empty() ? "(none)" : animator->currentClipName.c_str()))
			{
				for (const auto& c : clips)
				{
					animator->currentClipName = c->name;
					animator->playbackTime = 0.0f;
					m_selectedWindow = m_selectedPoint = -1;
				}
				ImGui::EndCombo();	
			}
		}

		auto clip = AnimationCache::get().getClip(meshPath, animator->currentClipName);
		if (!clip)
		{
			ImGui::TextDisabled(TR("select a clip"));
			ImGui::End();
			return;
		}

		const bool trackExists = lib.find(meshPath, clip->name) != nullptr;
		std::shared_ptr<ActionEventTrack> track = trackExists ? lib.edit(meshPath, clip->name) : nullptr;
		const float fps = track ? track->fps : 60.0f;
		const float totalFrames = std::max(1.0f, clip->duration * fps);
		const float curFrame = animator->playbackTime * fps;

		//============ Transport ===================
		auto scrubTo = [&](float frame)
			{
				frame = std::clamp(frame, 0.0f, clip->duration);
				animator->playing = false;
				animator->playbackTime = std::min(frame / fps, clip->duration);
				ActionEventSystem::resync(reg, selected, *animator, meshPath, clip->duration);
				scene->sampleAnimationPoses();// ポーズを更新する
			};

		if (ImGui::Button(animator->playing ? TR("Pause") : TR("Play")))
		{
			animator->playing = !animator->playing;
		}

		ImGui::SameLine();
		if (ImGui::Button("|<")) scrubTo(std::floor(curFrame) - 1.0f);
		ImGui::SameLine();
		if (ImGui::Button(">|")) scrubTo(std::floor(curFrame) + 1.0f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat(TR("Speed"), &animator->playbackSpeed, 0.01f, 0.05f, 4.0f, "%.2fx");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::SliderFloat(TR("Zoom"), &m_pixelPerFrame, 2.0f, 24.0f, "%.0f px/f");
	}
}

