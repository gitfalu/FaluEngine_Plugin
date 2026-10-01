#pragma once 
#include <FaluEngine/LLMClient.h>
#include <string>

namespace FaluEngine
{
	class FALU_ENGINE_API ClaudeApiClient : public ILLMClient
	{
	public:
		explicit ClaudeApiClient(std::string apikey, std::string model = "claude-sonnet-4-6");

		LLMDebugResponse sendDebugRequest(
			const LLMDebugRequest& request,
			const std::string& systemPrompt) override;

	private:
		std::string m_apiKey;
		std::string m_model;
	};
}
