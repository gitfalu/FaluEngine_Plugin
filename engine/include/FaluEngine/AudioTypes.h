#pragma once

struct IXAudio2SourceVoice;

namespace FaluEngine
{
	struct FALU_AUDIO_API AudioVoiceHandle
	{
		IXAudio2SourceVoice* voice = nullptr;
	};
}
