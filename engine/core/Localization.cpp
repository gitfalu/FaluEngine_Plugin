#include "FaluEngine/Localization.h"

#include <string>
#include <string_view>
#include <unordered_map>

/*
* string_view:所有権を持たない文字列(https://cpprefjp.github.io/reference/string_view.html)
* equal_to   :等値比較を行う関数オブジェクト。(https://cpprefjp.github.io/reference/functional/equal_to.html)
*/

namespace FaluEngine::Loc
{
	namespace
	{
		struct StringHash
		{
			using is_transparent = void;
			std::size_t operator()(std::string_view sv) const noexcept
			{
				return std::hash<std::string_view>()(sv);
			}
		};

		struct Translation
		{
			std::string ja;
			std::string en;
			bool hasJa = false;
			bool hasEn = false;
		};

		using TranslationMap = std::unordered_map<std::string, Translation, StringHash, std::equal_to<>>;
		using ResolveMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

		TranslationMap& table()
		{
			static TranslationMap t;
			return t;
		}

		ResolveMap& cache(Language lang)
		{
			static ResolveMap caches[2];
			return caches[static_cast<int>(lang)];
		}

		Language g_language = Language::English;

		std::string resolve(std::string_view key, Language lang)
		{
			const std::size_t pos = key.find("##");
			const std::string_view base = key.substr(0, pos);
			const std::string_view suffix = (pos == std::string_view::npos) ? 
				std::string_view{} : key.substr(pos);

			if (base.empty()) return std::string(key);

			const auto it = table().find(base);
			if (it == table().end()) return std::string(key);

			const Translation& t = it->second;
			const std::string* text = nullptr;
			if (lang == Language::Japanese && t.hasJa) text = &t.ja;
			if (lang == Language::English && t.hasEn) text = &t.en;
			if (!text) return std::string(key);

			std::string out = *text;
			out.append(suffix);
			return out;
		}
	}
	
	// namespace
	void setLanguage(Language lang) { g_language = lang; }
	
	Language getLanguage() { return g_language; }

	void addEntries(const Entry* entries, std::size_t count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (!entries[i].key) continue;
			Translation& t = table()[entries[i].key];
			if (entries[i].ja) { t.ja = entries[i].ja; t.hasJa = true; }
			if (entries[i].en) { t.en = entries[i].en; t.hasEn = true; }

			cache(Language::English).clear();
			cache(Language::Japanese).clear();
		}
	}

	const char* tr(const char* key)
	{
		if (!key) return "";

		ResolveMap& c = cache(g_language);
		const auto it = c.find(std::string_view(key));
		if (it != c.end()) return it->second.c_str();

		return c.emplace(key, resolve(key, g_language)).first->second.c_str();
	}
}
