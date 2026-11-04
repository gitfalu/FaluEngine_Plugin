#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class FALU_CORE_API ScriptProjectGenerator
	{
	public:
		struct Config
		{
			std::filesystem::path scriptRoot;	// 探索ルート
			std::filesystem::path projectDir;	// sln/vcxprojの出力先
			std::filesystem::path sdkDir;		// <root>/sdk
			std::filesystem::path outputDir;	// GameCode.dllの出力先
			std::filesystem::path templateDir;	// assets/templates
			std::string projectName = "GameCode";
		};

		struct Result
		{
			bool ok = false;
			bool changed = false;
			std::string error;
		};

		static ScriptProjectGenerator& get();

		void init(Config cfg);
		[[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

		/// @brief スクリプトを探索してsln/vcxproj/filters/GameModule.cppを生成。内容が同じならファイルは触らない
		/// @return 
		Result regenerate();

		/// @brief テンプレートからネイティブスクリプトを作成してプロジェクトに登録する
		/// @param dir 作成先フォルダ
		/// @param className クラス名
		/// @param error 失敗理由
		/// @return 
		bool createNativeScript(const std::filesystem::path& dir, const std::string& className, std::string& error);

		[[nodiscard]] static bool isValidClassName(const std::string& name);

		/// @brief MSBuildをvswhereで探す(見つからなければ空)。結果はキャッシュ
		/// @return 
		[[nodiscard]] static std::filesystem::path findMSBuild();

		[[nodiscard]] std::filesystem::path solutionPath() const;
		[[nodiscard]] std::filesystem::path projectPath() const;

		/// @brief ソースファイルを関連つけっられたアプリ(VisualStudio)で開く
		/// @return 
		bool openSolution() const;
		static bool openFile(const std::filesystem::path& file);

	private:
		ScriptProjectGenerator() = default;

		struct SourceSet
		{
			std::vector<std::filesystem::path> compile; // .cpp
			std::vector<std::filesystem::path> include; // .h/.cpp/.inl
		};

		SourceSet scan() const;
		std::string buildVcxproj(const SourceSet& s) const;
		std::string buildFilters(const SourceSet& s) const;
		std::string buildSolution() const;
		std::string loadTemplate(const char* fileName, const char* fallback) const;

		Config m_cfg;
		bool m_initialized = false;
	};
}