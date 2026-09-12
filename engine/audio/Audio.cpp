#include <FaluEngine/Audio.h>
#include "audio/AudioEngine.h"

namespace FaluEngine
{
	void playSound(const std::string& path, float volume, bool loop)
	{
		AudioEngine::get().play(path, volume, loop);
	}
}
