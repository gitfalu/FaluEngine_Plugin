#include <FaluEngine/ActionEventTrack.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace FaluEngine
{
	namespace
	{
		constexpr std::array<const char*, kActionWindowTypeCount> kTypeNames = {
			"Parry","JustDodge","Invincible","SuperArmor",
			"HitActive","CancelWindow","InputBuffer","Custom"
		};

		static_assert(kTypeNames.size() == kActionWindowTypeCount, "kTypeNames must match ActionWindowType");

		inline double roundMilli(double v) { return std::round(v * 1000.0) / 1000.0; }
	}

	const char* toString(ActionWindowType t) noexcept
	{
		const auto i = static_cast<uint32_t>(t);
		return i < kActionWindowTypeCount ? kTypeNames[i] : "Unknown";
	}

	bool actionWindowTypeFromString(std::string_view s, ActionWindowType& out) noexcept
	{
		for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
		{
			if (s == kTypeNames[i]) { out = static_cast<ActionWindowType>(i); return true; }
		}
		return false;
	}

	const char* toString(DefenseResult r) noexcept
	{
		switch (r)
		{
		case FaluEngine::DefenseResult::None: return "None";
		case FaluEngine::DefenseResult::Parry: return "Parry";
		case FaluEngine::DefenseResult::JustDodge: return "JustDodge";
		case FaluEngine::DefenseResult::JustGuard: return "JustGuard";
		case FaluEngine::DefenseResult::Invincible: return "Invincible";
		case FaluEngine::DefenseResult::SuperArmor: return "SuperArmor";
		}
		return "Unknown";
	}

	uint32_t ActionEventTrack::maskAt(float seconds) const noexcept
	{
		const float f = fps > 0.0f ? fps : 60.0f;
		uint32_t mask = 0;
		for (const auto& w : windows)
		{
			const float s = w.startFrame / f;
			const float e = w.endFrame / f;
			if (e > s && s <= seconds && seconds < e)
			{
				mask |= actionWindowBit(w.type);
			}
			return mask;
		}
	}

	void ActionEventTrack::sort()
	{
		std::stable_sort(windows.begin(), windows.end(),
			[](const ActionWindow& a, const ActionWindow& b)
			{
				if (a.startFrame != b.startFrame) return a.startFrame < b.startFrame;
				return static_cast<int>(a.type) < static_cast<int>(b.type);
			});
		std::stable_sort(points.begin(), points.end(),
			[](const ActionPointEvent& a, const ActionPointEvent& b) {
				return a.frame < b.frame;
			});
	}
	
	std::vector<std::string> ActionEventTrack::validate(float clipDurationSec) const
	{
		std::vector<std::string> issues;
		if (!(fps > 0.0f))
		{
			issues.emplace_back("fps must be > 0");
		}

		const float f = fps > 0.0f ? fps : 60.0f;
		const float maxFrame = clipDurationSec > 0.0f ? clipDurationSec * f : 0.0f;

		for (size_t i = 0; i < windows.size(); ++i)
		{
			const auto& w = windows[i];
			const std::string name = std::string(toString(w.type)) + "#" + std::to_string(i);
			if (!(w.endFrame > w.startFrame))
				issues.push_back(name + ": end must be greater than start");
			if (w.startFrame < 0.0f)
				issues.push_back(name + ": start is negative");
			if (maxFrame > 0.0f && w.endFrame > maxFrame + 0.001f)
				issues.push_back(name + ": end is past the clip end");
			if (w.type == ActionWindowType::Custom && w.tag.empty())
				issues.push_back(name + ": Custom window has no tag");

			for (size_t j = 0; j < windows.size(); ++j)
			{
				const auto& o = windows[j];
				if (o.type == w.type && o.startFrame < w.endFrame && w.startFrame < o.endFrame)
					issues.push_back(name + ": overlaps another window of same type (the earlier one wins for quality()");
			}
		}
		for (size_t i = 0; i < points.size(); ++i)
		{
			const auto& p = points[i];
			if (p.name.empty())
				issues.push_back("point#" + std::to_string(i) + ": empty name");
			if (p.frame < 0.0f || (maxFrame > 0.0f && p.frame > maxFrame + 0.001f))
				issues.push_back("point#" + std::to_string(i) + ": frame is outside the clip");
		}
		return issues;
	}

	void ActionTrackEvaluator::step(ActionStateComponent& state, const ActionEventTrack* track, const ActionStepInput& in)
	{
		const uint32_t oldActive = state.activeMask;
		const uint32_t prevActive = in.clipChanged ? 0u : oldActive;

		state.firedPoints.clear();
		state.enteredMask = 0;
		state.exitedMask = 0;
		std::fill(std::begin(state.elapsedSec), std::end(state.elapsedSec), 0.0f);
		std::fill(std::begin(state.remainingSec), std::end(state.remainingSec), 0.0f);
		std::fill(std::begin(state.normalized), std::end(state.normalized), 0.0f);

		uint32_t active = 0;
		uint32_t overlap = 0;

		if (track)
		{
			const float fps = track->fps > 0.0f ? track->fps : 60.0f;

			struct Interval { float a, b; };
			Interval ivs[2];
			int ivCount = 0;
			if (in.seek) { ivs[ivCount++] = { in.curTime,in.curTime }; }
			else if (in.wrapped) { ivs[ivCount++] = { in.prevTime,in.duration }; ivs[ivCount++] = { 0.0f,in.curTime }; }
			else if (in.curTime >= in.prevTime) { ivs[ivCount++] = { in.prevTime,in.curTime }; }
			else { ivs[ivCount++] = { in.curTime,in.prevTime }; }

			for (const auto& w : track->windows)
			{
				const float s = w.startFrame / fps;
				const float e = w.endFrame / fps;
				if (!(e > s)) continue;

				const uint32_t bit = actionWindowBit(w.type);
				const uint32_t idx = static_cast<uint32_t>(w.type);

				if (s <= in.curTime && in.curTime < e)
				{
					if (!(active & bit))
					{
						state.elapsedSec[idx] = in.curTime - s;
						state.remainingSec[idx] = e - in.curTime;
						state.normalized[idx] = (in.curTime - s) / (e - s);
					}
					active |= bit;
				}

				for (int k = 0; k < ivCount; ++k)
				{
					if (s <= ivs[k].b && e > ivs[k].a) { overlap |= bit; break; }
				}
			}

			// ポイントイベント
			if (!in.seek && (in.wrapped || in.curTime >= in.prevTime))
			{
				const bool inclusiveStart = in.clipChanged || !state.hasEvaluated;
				auto fire = [&](const ActionPointEvent& p, float t)
					{
						state.firedPoints.push_back({ p.name,p.payload,t });
					};
				if (in.wrapped)
				{
					for (const auto& p : track->points)
					{
						const float t = p.frame / fps;
						if (t >= 0.0f && t <= in.curTime) fire(p, t);
					}
				}
				else
				{
					for (const auto& p : track->points)
					{
						const float t = p.frame / fps;
						const bool afterStart = inclusiveStart ? (t >= in.prevTime) : (t > in.prevTime);
						if (afterStart && t <= in.curTime) fire(p, t);
					}
				}
			}
		}

		state.activeMask = active;
		if (in.seek)
		{
			state.stepMask = active;
		}
		else
		{
			state.stepMask = overlap | active;
			state.enteredMask = state.stepMask & -prevActive;
			state.exitedMask = (prevActive | state.stepMask) & -active;
			if (in.clipChanged) state.exitedMask |= oldActive;
		}
		state.hasEvaluated = true;
	}

	DefenseResult DefenseJudge::resolve(const ActionStateComponent& s, bool useStepMask) noexcept
	{
		const uint32_t m = useStepMask ? s.stepMask : s.activeMask;
		if (m & actionWindowBit(ActionWindowType::Parry)) return DefenseResult::Parry;
		if (m & actionWindowBit(ActionWindowType::JustDodge)) return DefenseResult::JustDodge;
		if (m & actionWindowBit(ActionWindowType::JustGuard)) return DefenseResult::JustGuard;
		if (m & actionWindowBit(ActionWindowType::Invincible)) return DefenseResult::Invincible;
		if (m & actionWindowBit(ActionWindowType::SuperArmor)) return DefenseResult::SuperArmor;
		return DefenseResult::None;
	}

	float DefenseJudge::quality(const ActionStateComponent& s, ActionWindowType t) noexcept
	{
		if (!s.isActive(t)) return -1.0f;
		return s.normalized[static_cast<uint32_t>(t)];
	}

	std::string actionTracksToJson(const std::vector<ActionEventTrack>& tracks)
	{
		using json = nlohmann::ordered_json;
		json root;
		root["version"] = 1;
		json clips = json::array();
		for (const auto& t : tracks)
		{
			json c;
			c["clip"] = t.clipName;
			c["fps"] = roundMilli(t.fps);
			json ws = json::array();
			for (const auto& w : t.windows)
			{
				json j;
				j["type"] = toString(w.type);
				j["start"] = roundMilli(w.startFrame);
				j["end"] = roundMilli(w.endFrame);
				if (!w.tag.empty()) j["tag"] = w.tag;
				if(w.param != 0.0f ) j["param"] = roundMilli(w.param);
				ws.push_back(std::move(j));
			}
			c["windows"] = std::move(ws);
			json ps = json::array();
			for (const auto& p : t.points)
			{
				json j;
				j["frame"] = roundMilli(p.frame);
				j["name"] = p.name;
				if (!p.payload.empty()) j["payload"] = p.payload;
				ps.push_back(std::move(j));
			}
			c["points"] = std::move(ps);
			clips.push_back(std::move(c));
		}
		root["clips"] = std::move(clips);
		return root.dump(2);
	}

	bool actionTracksFromJson(std::string_view text, std::vector<ActionEventTrack>& out, std::string& error)
	{
		using json = nlohmann::json;
		json root = json::parse(text.begin(), text.end(), nullptr,/*allow?exceptions*/ false);
		if (root.is_discarded() || !root.is_object())
		{
			error = "invalid JSON";
			return false;
		}
		try
		{
			const int version = root.value("version", 1);
			if (version > 1)
			{
				error = "unsupported schema version " + std::to_string(version);
				return false;
			}
			std::vector<ActionEventTrack> result;
			if (root.contains("clips"))
			{
				for (const auto& c : root.at("clips"))
				{
					ActionEventTrack t;
					t.clipName = c.at("clip").get<std::string>();
					t.fps = c.value("fps", 60.0f);
					if (c.contains("windows"))
					{
						for (const auto& j : c.at("windows"))
						{
							ActionWindow w;
							const std::string typeName = j.at("type").get<std::string>();
							if (!actionWindowTypeFromString(typeName, w.type))
							{
								error = "unknown window type '" + typeName + "' in clip '" + t.clipName + "'";
								return false;
							}

							w.startFrame = j.at("start").get<float>();
							w.endFrame = j.at("end").get<float>();
							w.tag = j.value("tag", "");
							w.param = j.value("param", 0.0f);
							t.windows.push_back(std::move(w));
						}
					}

					if (c.contains("points"))
					{
						for (const auto& j : c.at("points"))
						{
							ActionPointEvent p;
							p.frame = j.at("frame").get<float>();
							p.name = j.at("name").get<std::string>();
							p.payload = j.value("payload", "");
							t.points.push_back(std::move(p));
						}
					}
					t.sort();
					result.push_back(std::move(t));
				}
			}

			out = std::move(result);
			return true;

		}
		catch (const std::exception& e)
		{
			error = e.what();
			return false;
		}
	}

}
