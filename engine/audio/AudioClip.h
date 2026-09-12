#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <Windows.h>
#include <mmeapi.h>
#include "asset/AssetManager.h"
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	struct FALU_ENGINE_API AudioClip : public Asset
	{
		WAVEFORMATEX format{};
		std::vector<uint8_t> pcmData;

		static std::shared_ptr<AudioClip> load(const std::string& path);
	};

}


