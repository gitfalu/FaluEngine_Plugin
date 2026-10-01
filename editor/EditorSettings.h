#pragma once 

#include <string>

namespace Editor
{
	class EditorSettings
	{
	public:
		static EditorSettings& get();

		void load();
		void save() const;

		[[nodiscard]] const std::string& getClaudeApiKey() const noexcept { return m_claudeApiKey; }

		void setClaudeApiKey(std::string key) { m_claudeApiKey = std::move(key); }

		[[nodiscard]] const std::string& getClaudeModel() const noexcept { return m_claudeModel; }

		void setClaudeModel(std::string model) { m_claudeModel = std::move(model); }
	private:
		EditorSettings() = default;

		std::string m_claudeApiKey;
		std::string m_claudeModel = "claude-sonnet-4-6";
	};
}
