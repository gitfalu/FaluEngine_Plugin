/*****************************************************************//**
 * \file   launcher/main.cpp
 * \brief  
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <string>

#ifndef FALU_MODULE_NAME
#error "FALU_MODULE_NAME must be defined (e.g. FaluEditor)"
#endif

#define FALU_STR2(x) #x
#define FALU_STR(x) FALU_STR2(x)
#define FALU_WIDEN2(x) L##x
#define FALU_WIDEN(x) FALU_WIDEN2(x)
#define FALU_CAT2(a,b) a##b
#define FALU_CAT(a,b) FALU_CAT2(a,b)


static void fail(const std::wstring& message)
{
	std::fwprintf(stderr, L"%ls\n", message.c_str());
	MessageBoxW(nullptr, message.c_str(), L"FaluEngine", MB_OK | MB_ICONERROR);
}

int main()
{
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	const std::filesystem::path moduleDir = std::filesystem::path(exePath).parent_path() / L"modules";
	const std::filesystem::path modulePath = moduleDir / (FALU_WIDEN(FALU_STR(FALU_MODULE_NAME))L".dll");

	SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	AddDllDirectory(moduleDir.c_str());

	HMODULE module = LoadLibraryExW(modulePath.c_str(), nullptr,
		LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if (!module)
	{
		fail(L"Failed to load:\n" + modulePath.wstring() + L"\n\nWin32 error " + std::to_wstring(GetLastError()) +
			+L"\n(modules/ フォルダがない、または依存DLLが不足している)");
		return 1;
	}

	using EntryFn = int (*)();
	auto entry = reinterpret_cast<EntryFn>(GetProcAddress(module, FALU_STR(FALU_CAT(FALU_MODULE_NAME, Main))));
	if (!entry)
	{
		fail(L"Entry point not found: " FALU_WIDEN(FALU_STR(FALU_CAT(FALU_MODULE_NAME, Main))));
		return 1;
	}

	return entry();
}
