#pragma once
#include <FaluEngine/EngineExport.h>
#include <cstddef>

/// @brief コードに書かれた文字列を英語と日本語に変換。リストにないものはそのまま返す
namespace FaluEngine::Loc
{
	enum class Language { English = 0, Japanese = 1, };

	struct Entry
	{
		const char* key;
		const char* ja;
		const char* en;
	};

	FALU_CORE_API void setLanguage(Language lang);
	FALU_CORE_API Language getLanguage();
	FALU_CORE_API void addEntries(const Entry* entries, std::size_t count);
	FALU_CORE_API const char* tr(const char* key);
}

#define TR(text) ::FaluEngine::Loc::tr(text)
