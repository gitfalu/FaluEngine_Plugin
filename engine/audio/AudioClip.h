#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <Windows.h>
#include <mmeapi.h>
#include "asset/AssetManager.h"

namespace FaluEngine
{
	struct AudioClip : public Asset
	{
		WAVEFORMATEX format{};
		std::vector<uint8_t> pcmData;

		static std::shared_ptr<AudioClip> load(const std::string& path);
	};

}


