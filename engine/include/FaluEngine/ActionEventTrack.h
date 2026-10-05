/*****************************************************************//**
 * \file   ActionEventTrack.h
 * \brief  
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/
#include <FaluEngine/EngineExport.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace FaluEngine
{
	enum class ActionWindowType : uint8_t
	{
		Parry = 0,     // パリィ受付
		JustGuard,     // ジャストガード受付
		JustDodge,     // ジャスト回避(ギリギリ回避)受付
		Invincible,    // 無敵
		SuperArmor,    // スーパーアーマー(怯まない)
		HitActive,     // 攻撃判定が有効な区間
		CancelWindow,  // 次の行動へキャンセル可能な区間
		InputBuffer,   // 先行入力を受け付ける区間
		Custom,        // ユーザー定義(tag で区別)
		Count
	};

	inline constexpr uint32_t kActoinWindowTypeCount = static_cast<uint32_t>(ActionWindowType::Count);
	static_assert(kActoinWindowTypeCount <= 32, "ActionWindowType must fit in a uint32_t bitmask");

	[[nodiscard]] constexpr uint32_t actionWindowBit(ActionWindowType t) noexcept
	{
		return 1u << static_cast<uint32_t>(t);
	}

	FALU_ENGINE_API const char* toString(ActionWindowType t)noexcept;
	FALU_ENGINE_API bool actionWindowTypeFromString(std::string_view s, ActionWindowType& out) noexcept;

	struct ActionWindow
	{
		actionWindowType type = ActionWindowType::Parry;
		float startFrame = 0.0f; // 含む
		float endFrame = 1.0f; // 含まない
		std::string tag; // Customの識別名
		float param = 0.0f; // 任意パラメーター
	};

	struct ActionPointEvent
	{
		float frame = 0.0f;
		std::string name;
		std::string payload;
	};

	struct ActionEventTrack
	{
		std::string clipName;
		float fps = 60.0f;
		std::vector<ActionWindow> windows;
		std::vector<ActionPointEvent> points;

		[[nodiscard]] float toSeconds(float frame) const noexcept { return frame / fps; }
		[[nodiscard]] float toFrame(float seconds) const noexcept { return seconds * fps; }


	};
}
