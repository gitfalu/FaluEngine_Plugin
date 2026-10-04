#include "AnimationEventPanel.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Component.h"
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
		}
	}
}

