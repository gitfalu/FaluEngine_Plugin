#include "AudioClip.h"
#include "core/Logger.h"
#include <fstream>
#include <cstring>

namespace FaluEngine
{
	namespace {
		bool matchTag(const char* a, const char* b) {
			return std::memcmp(a, b, 4) == 0;
		}
	}

	std::shared_ptr<AudioClip> AudioClip::load(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			LOG_ERROR("AudioClip: failed to open '{}'", path);
			return nullptr;
		}

		char riffTag[4];
		file.read(riffTag, 4);
		if (!matchTag(riffTag, "RIFF"))
		{
			LOG_ERROR("AudioClip: '{}' is not a RIFF/WAVE file", path);
			return nullptr;
		}

		uint32_t riffSize = 0;
		file.read(reinterpret_cast<char*>(&riffSize), 4);

		char waveTag[4];
		file.read(waveTag, 4);
		if (!matchTag(waveTag, "WAVE"))
		{
			LOG_ERROR("AudioClip: '{}' is missing WAVE tag", path);
			return nullptr;
		}

		auto clip = std::make_shared<AudioClip>();
		bool hasFmt = false;
		bool hasData = false;

		while (file && !(hasFmt && hasData))
		{
			char chunkId[4];
			uint32_t chunkSize = 0;
			file.read(chunkId, 4);
			file.read(reinterpret_cast<char*>(&chunkSize), 4);
			if (!file) break;

			if (matchTag(chunkId, "fmt "))
			{
				std::vector<uint32_t> fmtBuf(chunkSize);
				file.read(reinterpret_cast<char*>(fmtBuf.data()), chunkSize);
				
				std::memset(&clip->format, 0, sizeof(WAVEFORMATEX));
				std::memcpy(&clip->format, fmtBuf.data(),
					std::min<size_t>(sizeof(WAVEFORMATEX), fmtBuf.size()));
				hasFmt = true;
			}
			else if (matchTag(chunkId,"data"))
			{
				clip->pcmData.resize(chunkSize);
				file.read(reinterpret_cast<char*>(clip->pcmData.data()), chunkSize);
				hasData = true;
			}
			else
			{
				file.seekg(chunkSize + (chunkSize % 2), std::ios::cur);
			}
		}

		if (!hasFmt || !hasData)
		{
			LOG_ERROR("AudioClip: '{}' missing fmt/data chunk (non-PCM WAV is not supported yet)", path);
			return nullptr;
		}

		clip->path = path;
		clip->loaded = true;
		LOG_INFO("AudioClip loaded: {} ({} ch, {} Hz, {} bytes)",
			path, clip->format.nChannels, clip->format.nSamplesPerSec, clip->pcmData.size());

		return clip;
	}
}
