#pragma once 

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "FaluEngine/LLMClient.h"
#include "llm/FrameContextBuilder.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace FaluEngine
{
	class Scene;
	class ReplayRecorder;
	class RenderTexture;
}

namespace Editor
{
	class LLMDebugPanel
	{
	public:
		void draw(
			FaluEngine::ReplayRecorder* replayRecorder,
			FaluEngine::Scene* activeScene,
			bool isPaused,
			uint64_t currentScrubFrame,
			ID3D11Device* device,
			ID3D11DeviceContext* context,
			const FaluEngine::RenderTexture* currentRenderTarget
		);

	private:
		struct ChatEntry
		{
			bool isUser = false;
			std::string text;
		};

		void ensureClientUpToDate();

		FaluEngine::FrameContextBuilder m_builder;
		std::unique_ptr<FaluEngine::ILLMClient> m_client;
		std::string m_clientKeyCache;

		char m_questionBuffer[512] = "";
		int m_frameRangeBack = 30;

		bool m_includeScreenshot = true;

		std::vector<ChatEntry> m_history;
		bool m_requestInFlight = false;
		std::string m_lastError;
	};
}
