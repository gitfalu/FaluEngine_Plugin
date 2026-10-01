#pragma once 
#include <string>
#include <vector>
#include <cstdint>
#include <FaluEngine/EngineExport.h>


namespace FaluEngine
{
	/// @brief LLM‚É‘—‚é‰æ‘œˆê–‡•ª‚Ìƒf[ƒ^
	struct FALU_ENGINE_API LLMImageAttachment
	{
		std::vector<uint8_t> pngData;
		std::string label;
	};

	struct FALU_ENGINE_API LLMDebugRequest
	{
		std::string prompt;
		std::vector<LLMImageAttachment> images;
	};

	struct FALU_ENGINE_API LLMDebugResponse
	{
		bool success = false;
		std::string text;
		std::string error;
	};

	class FALU_ENGINE_API ILLMClient
	{
	public:
		virtual ~ILLMClient() = default;

		virtual LLMDebugResponse sendDebugRequest(
			const LLMDebugRequest& request,
			const std::string& systemPrompt) = 0;
	};

}
