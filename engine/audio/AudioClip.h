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
	struct FALU_AUDIO_API AudioClip : public Asset
	{
		WAVEFORMATEX format{}; // Waveデータ
		std::vector<uint8_t> pcmData;// パルス符号変調の動的配列
		/// @brief 音声ファイルをファイルパスから読み込み
		/// @param path 音声ファイルパス
		/// @return 
		static std::shared_ptr<AudioClip> load(const std::string& path);
	};

}


