#pragma once 
#include <FaluEngine/EngineExport.h>
#include <string>
#include <cstdint>

namespace FaluEngine
{
	FALU_ENGINE_API void drawText(float x, float y,const std::string& text, uint32_t colorABGR = 0xFFFFFFFF);
}
