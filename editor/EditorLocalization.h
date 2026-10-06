#pragma once

#include "FaluEngine/Localization.h"

namespace Editor
{
	/// @brief 翻訳表の登録と保存済み言語の適用
	void initLocalization();

	/// @brief 表示言語を切り替えて、editor_settings.jsonに保存する
	/// @param lang 
	void setLanguage(FaluEngine::Loc::Language lang);
}
