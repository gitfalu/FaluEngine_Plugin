#include "EditorSettings.h"
#include "core/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>

namespace Editor
{
	namespace
	{
		constexpr const char* kSettingsPath = "editor_settings.json";
	}

	EditorSettings& EditorSettings::get()
	{
		static EditorSettings instance;
		return instance;
	}

	void EditorSettings::load()
	{
		std::ifstream in(kSettingsPath);
		if (!in.is_open())
		{
			return;
		}

		try
		{
			nlohmann::json j;
			in >> j;

			m_claudeApiKey = j.value("caludeApiKey", m_claudeApiKey);
			m_claudeModel = j.value("claudeModel", m_claudeModel);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("EditorSettings: failed to parse {}: {}", kSettingsPath, e.what());
		}
	}

	void EditorSettings::save() const
	{
		nlohmann::json j;
		j["claudeApiKey"] = m_claudeApiKey;
		j["claudeModel"] = m_claudeModel;

		std::ofstream out(kSettingsPath);
		if (!out.is_open())
		{
			LOG_ERROR("EditorSetting: failed to open {} for writing.", kSettingsPath);
			return;
		}

		out << j.dump(4);
	}
}
