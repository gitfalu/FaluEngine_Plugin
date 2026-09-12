#pragma once
#include <string>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	FALU_ENGINE_API void playSound(const std::string& path, float volume = 1.0f, bool loop = false);
}
