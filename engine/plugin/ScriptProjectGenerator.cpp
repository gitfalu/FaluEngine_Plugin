#include "ScriptProjectGenerator.h"
#include "core/Logger.h"
#include "core/PathResolver.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

namespace fs = std::filesystem;

namespace FaluEngine
{
	namespace
	{
		const char* kUtf8Bom = "\xEF\xBB\xBF";

		std::string readAll(const fs::path& p)
		{
			std::ifstream f(p, std::ios::binary);
			if (!f) return {};
			std::ostringstream ss;
			ss << f.rdbuf();
			return ss.str();
		}

		bool writeIfChanged(const fs::path& p, const std::string& content, bool& changed)
		{
			std::error_code ec;
			if (fs::exists(p, ec) && readAll(p) == content) return true;

			fs::create_directories(p.parent_path(), ec);
			std::ofstream f(p, std::ios::binary | std::ios::trunc);
			if (!f) return false;
			f.write(content.data(), static_cast<std::streamsize>(content.size()));
			changed = true;
			return f.good();
		}

		std::string toCrlf(const std::string& s)
		{
			std::string out;
			out.reserve(s.size() + s.size() / 16);
			for (size_t i = 0; i < s.size(); ++i)
			{
				if (s[i] == '\r') continue;
				if (s[i] == '\n') out += '\r';
				out += s[i];
			}
			return out;
		}

		std::string xmlEscape(const std::string& s)
		{
			std::string o;
			o.reserve(s.size());
			for (char c : s)
			{
				switch (c)
				{
				case '&': o += "&amp;"; break;
				case '<': o += "&lt;"; break;
				case '>': o += "&gt;"; break;
				case '"': o += "&quot;"; break;
				default: o += c;
				}
			}
			return o;
		}

		std::string replaceAll(std::string s, const std::string& from, const std::string& to)
		{
			size_t pos = 0;
			while ((pos = s.find(from, pos)) != std::string::npos)
			{
				s.replace(pos, from.size(), to);
				pos += to.size();
			}
			return s;
		}

		std::string pathUtf8(const fs::path& p) { return PathResolver::toUtf8(p); }

		std::string relativeTo(const fs::path& target, const fs::path& base)
		{
			std::error_code ec;
			auto rel = fs::relative(target, base, ec);
			if (ec || rel.empty()) rel = target;
			std::string s = pathUtf8(rel);
			std::replace(s.begin(), s.end(), '/','\\');
			return s;
		}

		/// @brief 名前から決定的なGUIDを作る
		/// @param name 
		/// @return 
		std::string guidFromName(const std::string& name)
		{
			auto fnv = [&](uint64_t seed)
				{
					uint64_t h = 1469598103934665603ull ^ seed;
					for (unsigned char c : name) { h ^= c; h *= 1099511628211ull; }
					return h;
				};
			const uint64_t a = fnv(0), b = fnv(0x9E3779B97F4A7C15ull);
			char buf[64];
			snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%04X-%012llX}",
				static_cast<uint32_t>(a >> 32),static_cast<uint32_t>((a >> 16) & 0xFFFF),
				static_cast<uint32_t>(a & 0xFFFF),static_cast<uint32_t>(b >> 48),
				static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFull)
				);
			return buf;
		}

		std::string lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return (char)std::tolower(c); });
			return s;
		}

		bool isCompileExt(const std::string& e) { return e == ".cpp" || e == ".cc" || e == ".cxx"; }
		bool isIncludeExt(const std::string& e) { return e == ".h" || e == ".hpp" || e == ".hxx" || e == ".inl"; }

		std::string runCapture(std::wstring cmd)
		{
			SECURITY_ATTRIBUTES sa{ sizeof(sa),nullptr,TRUE };
			HANDLE r = nullptr, w = nullptr;
			if (!CreatePipe(&r, &w ,&sa, 0)) return {};
			SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);

			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = w;
			si.hStdError = w;
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

			PROCESS_INFORMATION pi{};
			const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
				CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
			CloseHandle(w);
			if (!ok) { CloseHandle(r); return {}; }

			std::string out;
			char buf[1024];
			DWORD n = 0;
			while (ReadFile(r, buf, sizeof(buf), &n, nullptr) && n > 0) out.append(buf, n);
			CloseHandle(r);
			WaitForSingleObject(pi.hProcess, 10000);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			return out;
		}

		const char* kFallbackHeader = R"(#pragma once
#include "FaluEngine/NativeScript.h"
#include "FaluEngine/Entity.h"

/// @brief {{CLASS_NAME}}
/// @date {{DATE}}
class {{CLASS_NAME}} : public FaluEngine::NativeScript
{
public:
	/// @brief シーン開始時(Play開始時)に一回呼ばれる
	void onInit(FaluEngine::Entity& entity) override;

	/// @brief 毎フレーム呼ばれる
	void onUpdate(FaluEngine::Entity& entity, float deltaTime) override;

	/// @brief 破棄時に呼ばれる
	void onDestroy(FaluEngine::Entity& entity) override;

	/// @brief ボタンクリック時に呼ばれる
	void onClick(FaluEngine::Entity& entity) override;
};
)";

		const char* kFallbackSource = R"(
#include "{{CLASS_NAME}}.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Scene.h"

void {{CLASS_NAME}}::onInit(FaluEngine::Entity& entity)
{
}

void {{CLASS_NAME}}::onUpdate(FaluEngine::Entity& entity ,float deltaTime)
{
}

void {{CLASS_NAME}}::onDestroy(FaluEngine::Entity& entity)
{
}

void {{CLASS_NAME}}::onClick(FaluEngine::Entity& entity)
{
}

// エディタのInspectorから名前で選べるように登録(必須)
REGISTER_NATIVE_SCRIPT({{CLASS_NAME}})
)";
		const char* kGameModuleSource = R"(
#include "FaluEngine/IPlugin.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class GameModule : public FaluEngine::IPlugin
{
public:
	[[nodiscard]] const char* getName() const noexcept override { return "GameCode"; }
	[[nodiscard]] const char* getVersion() const noexcept override { return "1.0"; }

	bool onLoad() override { return true; }
	void onUpdate(float deltaTime) override {}
	void onUnload() override { FaluEngine::NativeScriptRegistry::get().clear(); }

	std::vector<std::pair<std::string,
		std::function<std::unique_ptr<FaluEngine::NativeScript>()>>>
		getScriptFactories() override
	{
		return FaluEngine::NativeScriptRegistry::get().exportAll();
	}
};

extern "C" __declspec(dllexport) FaluEngine::IPlugin* createPlugin() { return new GameModule(); }
extern "C" __declspec(dllexport) void destroyPlugin(FaluEngine::IPlugin* plugin) { delete plugin; }
)";
	}

	ScriptProjectGenerator& ScriptProjectGenerator::get()
	{
		static ScriptProjectGenerator instance;
		return instance;
	}

	void ScriptProjectGenerator::init(Config cfg)
	{
		m_cfg = std::move(cfg);
		m_initialized = true;
	}

	fs::path ScriptProjectGenerator::projectPath() const
	{
		return m_cfg.projectDir / (m_cfg.projectName + ".vcxproj");
	}

	fs::path ScriptProjectGenerator::solutionPath() const
	{
		return m_cfg.projectDir / (m_cfg.projectName + ".sln");
	}

	bool ScriptProjectGenerator::isValidClassName(const std::string& name)
	{
		if (name.empty() || name.size() > 64) return false;
		if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) return false;
		for (char c : name)
			if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;

		static const std::set<std::string> reserved = {
			"class","struct","enum","union","namespace","template","typename","int","float","double","char",
			"bool","void","auto","const","static","virtual","public","private","protected","operator",
			"new","delete","this","return","if","else","for","while","switch","case","default","NativeScript"
		};
		return !reserved.contains(name);
	}

	ScriptProjectGenerator::SourceSet ScriptProjectGenerator::scan() const
	{
		SourceSet set;
		std::error_code ec;
		if (!fs::exists(m_cfg.scriptRoot, ec)) return set;

		// directory_iterator
		for (fs::recursive_directory_iterator it(m_cfg.scriptRoot, fs::directory_options::skip_permission_denied, ec), end;
			it != end; it.increment(ec))
		{
			if (ec) break;

			std::error_code e2;
			if (it->is_directory(e2))
			{
				const auto name = pathUtf8(it->path().filename());
				if (!name.empty() && name[0] == '.') it.disable_recursion_pending();
				continue;
			}
			if (!it->is_regular_file(e2)) continue;

			const auto ext = lower(pathUtf8(it->path().extension()));
			if (isCompileExt(ext)) set.compile.push_back(it->path());
			else if (isIncludeExt(ext)) set.include.push_back(it->path());
		}
		std::sort(set.compile.begin(), set.compile.end());
		std::sort(set.include.begin(), set.include.end());

		return set;
	}

	std::string ScriptProjectGenerator::buildVcxproj(const SourceSet& s) const
	{
		const std::string name = m_cfg.projectName;
		const std::string guid = guidFromName(name);
		const std::string sdkInc = xmlEscape(pathUtf8(m_cfg.sdkDir / "include"));
		const std::string sdkEng = xmlEscape(pathUtf8(m_cfg.sdkDir / "engine"));
		const std::string outDir = xmlEscape(pathUtf8(m_cfg.outputDir));

		auto libsFor = [&](const char* cfgName)
			{
				std::string libs;
				std::error_code ec;
				const fs::path dir = m_cfg.sdkDir / "lib" / cfgName;
				std::vector<std::string> names;
				if (fs::exists(dir, ec))
					for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec))
					{
						if (ec) break;
						if (lower(pathUtf8(it->path().extension())) == ".lib")
							names.push_back(pathUtf8(it->path().filename()));
					}
				std::sort(names.begin(), names.end());
				for (auto& n : names) libs += xmlEscape(n) + ";";
				return libs;
			};

		std::ostringstream o;
		o << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
			"<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
			"  <ItemGroup Label=\"ProjectConfigurations\">\r\n"
			"    <ProjectConfiguration Include=\"Debug|x64\"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>\r\n"
			"    <ProjectConfiguration Include=\"Release|x64\"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>\r\n"
			"  </ItemGroup>\r\n"
			"  <PropertyGroup Label=\"Globals\">\r\n"
			"    <VCProjectVersion>17.0</VCProjectVersion>\r\n"
			"    <ProjectGuid>" << guid << "</ProjectGuid>\r\n"
			"    <RootNamespace>" << name << "</RootNamespace>\r\n"
			"    <Keyword>Win32Proj</Keyword>\r\n"
			"    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\r\n"
			"  </PropertyGroup>\r\n"
			"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n";

		for (const char* cfgName : { "Debug","Release" })
		{
			const bool dbg = std::string(cfgName) == "Debug";
			o << "  <PropertyGroup Label=\"Configuration\" Condition=\"'$(Configuration)|$(Platform)'=='" << cfgName << "|x64'\">\r\n"
				"    <ConfigurationType>DynamicLibrary</ConfigurationType>\r\n"
				"    <UseDebugLibraries>" << (dbg ? "true" : "false") << "</UseDebugLibraries>\r\n"
				"    <PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>\r\n"
				"    <CharacterSet>Unicode</CharacterSet>\r\n"
				"  </PropertyGroup>\r\n";
		}

		o << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"
			"  <ImportGroup Label=\"ExtensionSettings\" />\r\n"
			"  <ImportGroup Label=\"PropertySheets\">\r\n"
			"    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"
			"  </ImportGroup>\r\n"
			"  <PropertyGroup>\r\n"
			"    <OutDir>" << outDir << "\\</OutDir>\r\n"
			"    <IntDir>$(ProjectDir)Intermediate\\$(Configuration)\\</IntDir>\r\n"
			"    <TargetName>" << name << "</TargetName>\r\n"
			"  </PropertyGroup>\r\n";

		for (const char* cfgName : { "Debug","Release" })
		{
			const bool dbg = std::string(cfgName) == "Debug";
			o << "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='" << cfgName << "|x64'\">\r\n"
				"    <ClCompile>\r\n"
				"      <WarningLevel>Level3</WarningLevel>\r\n"
				"      <ConformanceMode>true</ConformanceMode>\r\n"
				"      <LanguageStandard>stdcpp20</LanguageStandard>\r\n"
				"      <MultiProcessorCompilation>true</MultiProcessorCompilation>\r\n"
				"      <AdditionalOptions>/utf-8 /permissive- /wd4100 %(AdditionalOptions)</AdditionalOptions>\r\n"
				"      <AdditionalIncludeDirectories>" << sdkInc << ";" << sdkEng << ";%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\r\n"
				"      <PreprocessorDefinitions>GLM_ENABLE_EXPERIMENTAL;GLM_FORCE_DEPTH_ZERO_TO_ONE;NOMINMAX;WIN32_LEAN_AND_MEAN;_WINDOWS;_USRDLL;"
				<< (dbg ? "ENGINE_DEBUG;_DEBUG;" : "NDEBUG;") << "%(PreprocessorDefinitions)</PreprocessorDefinitions>\r\n"
				"      <RuntimeLibrary>" << (dbg ? "MultiThreadedDebugDLL" : "MultiThreadedDLL") << "</RuntimeLibrary>\r\n"
				"      <Optimization>" << (dbg ? "Disabled" : "MaxSpeed") << "</Optimization>\r\n"
				"      <DebugInformationFormat>" << "ProgramDatabase" << "</DebugInformationFormat>\r\n"
				"    </ClCompile>\r\n"
				"    <Link>\r\n"
				"      <SubSystem>Windows</SubSystem>\r\n"
				"      <GenerateDebugInformation>true</GenerateDebugInformation>\r\n"
				"      <AdditionalLibraryDirectories>" << xmlEscape(pathUtf8(m_cfg.sdkDir / "lib" / cfgName)) << ";%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\r\n"
				"      <AdditionalDependencies>" << libsFor(cfgName) << "%(AdditionalDependencies)</AdditionalDependencies>\r\n"
				"    </Link>\r\n"
				"  </ItemDefinitionGroup>\r\n";
		}

		o << "  <ItemGroup>\r\n";
		for (auto& p : s.include)
			o << "   <ClInclude Include=\"" << xmlEscape(relativeTo(p, m_cfg.projectDir)) << "\" />\r\n";
		o << "  </ItemGroup>\r\n <ItemGroup>\r\n";
		for (auto& p : s.compile)
			o << "   <ClCompile Include=\"" << xmlEscape(relativeTo(p, m_cfg.projectDir)) << "\" />\r\n";
		o << "    <ClCompile Include=\"Generated\\GameModule.cpp\" />\r\n"
			"  </ItemGroup>\r\n"
			"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n"
			"  <ImportGroup Label=\"ExtensionTargets\" />\r\n"
			"</Project>\r\n";
		return o.str();
	}

	std::string ScriptProjectGenerator::buildFilters(const SourceSet& s) const
	{
		std::set<std::string> filters;
		auto filterOf = [&](const fs::path& p)
			{
				std::error_code ec;
				auto rel = fs::relative(p.parent_path(), m_cfg.scriptRoot, ec);
				std::string f = (ec || rel.empty() || rel == ".") ? std::string() : pathUtf8(rel);
				std::replace(f.begin(), f.end(), '/', '\\');

				for (size_t pos = f.find('\\'); pos != std::string::npos; pos = f.find('\\', pos + 1))
					filters.insert(f.substr(0, pos));
				if (!f.empty()) filters.insert(f);
				return f;
			};

		std::ostringstream items;
		auto emit = [&](const char* tag, const std::vector<fs::path>& files)
			{
				for (auto& p : files)
				{
					const std::string f = filterOf(p);
					items << "   <" << tag << " Include=\"" << xmlEscape(relativeTo(p, m_cfg.projectDir)) << "\">\r\n";
					if (!f.empty()) items << "    <Filter>" << xmlEscape(f) << "</Filter>\r\n";
					items << "   </" << tag << ">\r\n";
				}
			};

		items << "  <ItemGroup>\r\n"; emit("ClInclude", s.include); items << "  </ItemGroup>\r\n";
		items << "  <ItemGroup>\r\n"; emit("ClCompile", s.compile);
		items << "   <ClCompile Include=\"Generated\\GameModule.cpp\"><Filter>Generated</Filter></ClCompile>\r\n";
		items << "  </ItemGroup>\r\n";
		filters.insert("Generated");

		std::ostringstream o;
		o << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
			"<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
			"  <ItemGroup>\r\n";
		for (auto& f : filters)
			o << "  <Filter Include=\"" << xmlEscape(f) << "\"><UniqueIdentifier>" << guidFromName("filter:" + f) << "</UniqueIdentifier></Filter>\r\n";
		o << " </ItemGroup>\r\n" << items.str() << "</Project>\r\n";
		return o.str();
	}

	std::string ScriptProjectGenerator::buildSolution() const
	{
		const std::string guid = guidFromName(m_cfg.projectName);
		const std::string sol = guidFromName("sln:" + m_cfg.projectName);
		std::ostringstream o;
		o << kUtf8Bom <<
			"\r\nMicrosoft Visual Studio Solution File, Format Version 12.00\r\n"
			"# Visual Studio Version 17\r\n"
			"VisualStudioVersion = 17.0.31903.59\r\n"
			"MinimumVisualStudioVersion = 10.0.40219.1\r\n"
			"Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" << m_cfg.projectName << "\", \""
			<< m_cfg.projectName << ".vcxproj\", \"" << guid << "\"\r\nEndProject\r\n"
			"Global\r\n"
			"\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n"
			"\t\tDebug|x64 = Debug|x64\r\n\t\tRelease|x64 = Release|x64\r\n"
			"\tEndGlobalSection\r\n"
			"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n";
		for (const char* c : { "Debug", "Release" })
			o << "\t\t" << guid << "." << c << "|x64.ActiveCfg = " << c << "|x64\r\n"
			<< "\t\t" << guid << "." << c << "|x64.Build.0 = " << c << "|x64\r\n";
		o << "\tEndGlobalSection\r\n"
			"\tGlobalSection(SolutionProperties) = preSolution\r\n\t\tHideSolutionNode = FALSE\r\n\tEndGlobalSection\r\n"
			"\tGlobalSection(ExtensibilityGlobals) = postSolution\r\n\t\tSolutionGuid = " << sol << "\r\n\tEndGlobalSection\r\n"
			"EndGlobal\r\n";
		return o.str();
	}

	ScriptProjectGenerator::Result ScriptProjectGenerator::regenerate()
	{
		Result r;
		if (!m_initialized) { r.error = "ScriptProjectGenerator is not initialized"; return r; }

		const SourceSet set = scan();
		bool changed = false;

		bool ok = true;
		ok &= writeIfChanged(m_cfg.projectDir / "Generated" / "GameModule.cpp", toCrlf(kGameModuleSource), changed);
		ok &= writeIfChanged(projectPath(), buildVcxproj(set), changed);
		fs::path filtersPath = projectPath();
		filtersPath += ".filters";
		ok &= writeIfChanged(filtersPath, buildFilters(set), changed);
		bool slnChanged = false;
		ok &= writeIfChanged(solutionPath(), buildSolution(), slnChanged);

		if (!ok)
		{
			r.error = "failed to write project files to " + pathUtf8(m_cfg.projectDir);
			LOG_ERROR("ScriptProject: {}", r.error);
			return r;
		}
		r.ok = true;
		r.changed = changed;
		if (changed)
			LOG_INFO("ScriptProject: regenerated {} ({} cpp / {} headers)", pathUtf8(solutionPath()), set.compile.size(), set.include.size());
		return r;
	}

	std::string ScriptProjectGenerator::loadTemplate(const char* fileName, const char* fallback) const
	{
		std::string t = readAll(m_cfg.templateDir / fileName);
		// BOMがあれば除く
		if (t.size() >= 3 && t.compare(0, 3, kUtf8Bom) == 0) t.erase(0, 3);
		return t.empty() ? std::string(fallback) : t;
	}

	bool ScriptProjectGenerator::createNativeScript(const fs::path& dir, const std::string& className, std::string& error)
	{
		if (!m_initialized) { error = "not initialized"; return false; }
		if (!isValidClassName(className))
		{
			error = "Invalid class name (use letters, digits and '_',not starting with a digit)";
			return false;
		}

		// scriptRoot以下のみ許可
		std::error_code ec;
		const auto root = fs::weakly_canonical(m_cfg.scriptRoot, ec);
		const auto target = fs::weakly_canonical(dir, ec);
		const auto rel = target.lexically_relative(root);
		if (rel.empty() || *rel.begin() == "..")
		{
			error = "Script must be created inside the assets folder";
			return false;
		}

		// 同盟のクラス(ファイル)がプロジェクト内にあるとレジストリ名が衝突するのではじく
		for(auto& p : scan().include)
			if (pathUtf8(p.stem()) == className) { error = "A script named '" + className + "' already exists"; return false; }
		for(auto& p :scan().compile)
			if (pathUtf8(p.stem()) == className) { error = "A script named '" + className + "' already exists"; return false; }

		const fs::path h = target / (className + ".h");
		const fs::path c = target / (className + ".cpp");
		if (fs::exists(h, ec) || fs::exists(c, ec)) { error = "File already exists"; return false; }


		const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm{};
		localtime_s(&tm, &now);
		char date[32];
		strftime(date, sizeof(date), "%Y-%m-%d", &tm);

		auto fill = [&](std::string s) {
			s = replaceAll(s, "{{CLASS_NAME}}", className);
			s = replaceAll(s, "{{DATE}}", date);
			return toCrlf(s);
			};

		const std::string hText = std::string(kUtf8Bom) + fill(loadTemplate("NativeScript.h.tmpl", kFallbackHeader));
		const std::string cText = std::string(kUtf8Bom) + fill(loadTemplate("NativeScript.cpp.tmpl", kFallbackSource));

		bool changed = false;
		if (!writeIfChanged(h, hText, changed) || !writeIfChanged(c, cText, changed))
		{
			error = "Failed to write script files";
			return false;
		}
		LOG_INFO("ScriptProject: created {}.h / {}.cpp", className, className);

		const auto r = regenerate();
		if (!r.ok) { error = r.error; return false; }
		return true;
	}

	fs::path ScriptProjectGenerator::findMSBuild()
	{
		static fs::path cached;
		static bool searched = false;
		if (searched) return cached;
		searched = true;

		wchar_t env[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"FALU_MSBUILD", env, MAX_PATH) > 0 && fs::exists(env))
			return cached = fs::path(env);

		wchar_t pf[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"ProgramFIles(x86)", pf, MAX_PATH) == 0)
			return cached;
		const fs::path vswhere = fs::path(pf) / L"Microsoft Visual Studio" / L"Installer" / L"vswhere.exe";
		if (!fs::exists(vswhere)) return cached;

		std::wstring cmd = L"\"" + vswhere.wstring() +
			L"\" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";
		std::string out = runCapture(cmd);

		// 先頭行だけ使う
		const auto nl = out.find_first_of("\r\n");
		if (nl != std::string::npos)out.erase(nl);
		if (out.empty()) return cached;

		fs::path p = PathResolver::fromUtf8(out);
		std::error_code ec;
		if (fs::exists(p, ec)) cached = p;
		return cached;
	}

	bool ScriptProjectGenerator::openFile(const fs::path& file)
	{
		const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
		return r > 32;
	}

	bool ScriptProjectGenerator::openSolution() const
	{
		if (!m_initialized) return false;
		std::error_code ec;
		if (!fs::exists(solutionPath(), ec)) return false;
		return openFile(solutionPath());
	}
}