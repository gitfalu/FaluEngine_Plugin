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

	inline constexpr uint32_t kActionWindowTypeCount = static_cast<uint32_t>(ActionWindowType::Count);
	static_assert(kActionWindowTypeCount <= 32, "ActionWindowType must fit in a uint32_t bitmask");

	[[nodiscard]] constexpr uint32_t actionWindowBit(ActionWindowType t) noexcept
	{
		return 1u << static_cast<uint32_t>(t);
	}

	FALU_ENGINE_API const char* toString(ActionWindowType t)noexcept;
	FALU_ENGINE_API bool actionWindowTypeFromString(std::string_view s, ActionWindowType& out) noexcept;

	struct ActionWindow
	{
		ActionWindowType type = ActionWindowType::Parry;
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

		[[nodiscard]] FALU_ENGINE_API uint32_t maskAt(float seconds) const noexcept;

		FALU_ENGINE_API void sort();

		[[nodiscard]] FALU_ENGINE_API std::vector<std::string> validate(float clipDurationSec) const;
	};

	//================ ランタイム状態
	struct ActionFiredPoint
	{
		std::string name;
		std::string payload;
		float timeSeconds = 0.0f;
	};

	/// @brief Entityごとのランタイム状態(ECSコンポーネントとして扱う)。シーン保存はしない。
	struct ActionStateComponent
	{
		//==== 現在の状態 =====
		uint32_t activeMask = 0; // 現在時間にアクティブな窓
		uint32_t stepMask = 0; // 直近の更新ステップ中に１度でも重なった窓
		uint32_t enteredMask = 0; // 今フレームで開いた窓
		uint32_t exitedMask = 0; // 今フレームで閉じた窓

		// 種類ごとの窓内進行度
		float elapsedSec[kActionWindowTypeCount] = {};
		float remainingSec[kActionWindowTypeCount] = {};
		float normalized[kActionWindowTypeCount] = {};

		std::vector<ActionFiredPoint> firedPoints;

		//======== キャッシュ =======
		std::shared_ptr<const ActionEventTrack> track;
		std::string cachedMeshPath;
		std::string cachedClipName;
		uint32_t cachedRevision = 0xFFFFFFFu;
		bool hasEvaluated = false;

		[[nodiscard]] bool isActive(ActionWindowType t) const noexcept { return (activeMask & actionWindowBit(t)) != 0; }

		[[nodiscard]] bool wasActionThisStep(ActionWindowType t) const noexcept { return (stepMask & actionWindowBit(t)) != 0; }
		[[nodiscard]] bool justEntered(ActionWindowType t) const noexcept { return (enteredMask & actionWindowBit(t)) != 0; }
		[[nodiscard]] bool justExited(ActionWindowType t) const noexcept { return (exitedMask & actionWindowBit(t)) != 0; }
	};

	//===== 評価器 ============-
	struct ActionStepInput
	{
		float prevTime = 0.0f; // 更新前の再生時間(秒)
		float curTime = 0.0f; // 更新後の再生時間(秒)
		float duration = 0.0f; // クリップ(秒)
		bool wrapped = false; // ループで折り返した
		bool seek = false; // 不連続ジャンプ(リプレイ巻き戻し等) : 状態のみ再計算、イベント客家はしない
		bool  clipChanged = false;
	};

	class FALU_ENGINE_API ActionTrackEvaluator
	{
	public:
		static void step(ActionStateComponent& state, const ActionEventTrack* track, const ActionStepInput& in);
	};

	//========== 防御判定 ============
	enum class DefenseResult : uint8_t
	{
		None = 0,
		Parry,
		JustDodge,
		JustGuard,
		Invincible,
		SuperArmor,
	};

	FALU_ENGINE_API const char* toString(DefenseResult r) noexcept;

	class FALU_ENGINE_API DefenseJudge
	{
	public:
		[[nodiscade]] static DefenseResult resolve(const ActionStateComponent& s, bool useStepMask = true) noexcept;


		[[nodiscard]] static float quality(const ActionStateComponent& s, ActionWindowType t) noexcept;
	};

	[[nodiscard]] FALU_ENGINE_API std::string actionTracksToJson(const std::vector<ActionEventTrack>& tracks);

	FALU_ENGINE_API bool actionTracksFromJson(std::string_view text, std::vector<ActionEventTrack>& out, std::string& error);
}
