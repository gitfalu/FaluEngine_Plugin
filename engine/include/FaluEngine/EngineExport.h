#pragma once
#if defined(_WIN32) && !defined(FALU_STATIC)
	#define FALU_DLL_EXPORT __declspec(dllexport)
	#define FALU_DLL_IMPORT __declspec(dllimport)
#else
	#define FALU_DLL_EXPORT
	#define FALU_DLL_IMPORT
#endif

#if defined(FALU_CORE_BUILD_DLL)
	#define FALU_CORE_API FALU_DLL_EXPORT
#else
	#define FALU_CORE_API FALU_DLL_IMPORT
#endif

#if defined(FALU_AUDIO_BUILD_DLL)
	#define FALU_AUDIO_API FALU_DLL_EXPORT
#else
	#define FALU_AUDIO_API FALU_DLL_IMPORT
#endif

#if defined(FALU_GRAPHICS_BUILD_DLL)
	#define FALU_GRAPHICS_API FALU_DLL_EXPORT
#else
	#define FALU_GRAPHICS_API FALU_DLL_IMPORT
#endif

#if defined(FALU_SCENE_BUILD_DLL)
	#define FALU_SCENE_API FALU_DLL_EXPORT
#else
	#define FALU_SCENE_API FALU_DLL_IMPORT
#endif

// ç≈è„à ÉÇÉWÉÖÅ[Éã(Application/GameLoop/LLMÅ@Ç»Ç«)
#if defined(FALU_ENGINE_BUILD_DLL)
	#define FALU_ENGINE_API FALU_DLL_EXPORT
#else
	#define FALU_ENGINE_API FALU_DLL_IMPORT
#endif
