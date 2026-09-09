#pragma once
#if defined(_WIN32)
	#if defined(FALU_ENGINE_BUILD_DLL)
		#define FALU_ENGINE_API __declspec(dllexport)
	#elif defined(FALU_ENGINE_USE_DLL)
		#define FALU_ENGINE_API __declspec(dllimport)
	#else 
		#define FALU_ENGINE_API
	#endif
#else
	#define FALU_ENGINE_API
#endif

