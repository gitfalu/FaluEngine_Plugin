#include "LLMDebugPanel.h"
#include "../EditorSettings.h"
#include "llm/Claudeapiclient.h"
#include "FaluEngine/ReplayRecorder.h"
#include "FaluEngine/Scene.h"

#include <imgui.h>

namespace Editor
{
	void LLMDebugPanel::ensureClientUpToDate()
	{
		const std::string& key = EditorSettings::get().getClaudeApiKey();
		if (!m_client || m_clientKeyCache != key)
		{
			m_client = std::make_unique<FaluEngine::ClaudeApiClient>(
				key, EditorSettings::get().getClaudeModel()
			);
		}
	}

	void LLMDebugPanel::draw(
		FaluEngine::ReplayRecorder* replayRecorder, 
		FaluEngine::Scene* activeScene, bool isPaused,
		uint64_t currentScrubFrame, 
		ID3D11Device* device, ID3D11DeviceContext* context, const FaluEngine::RenderTexture* currentRenderTarget)
	{
		if (!ImGui::Begin("LLM debug Assistant"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Settings"))
		{
			auto& settings = EditorSettings::get();

			static char keyBuffer[256] = "";
			static bool keyBufferInit = false;

			if (!keyBufferInit)
			{
				std::snprintf(keyBuffer, sizeof(keyBuffer), "%s", settings.getClaudeApiKey().c_str());
				keyBufferInit = true;
			}

			ImGui::TextUnformatted("Claude API Key");
			ImGui::InputText("##apikey", keyBuffer, sizeof(keyBuffer), ImGuiInputTextFlags_Password);

			if (ImGui::Button("Save"))
			{
				settings.setClaudeApiKey(keyBuffer);
				settings.save();
			}

			ImGui::TextDisabled("editor_settings.json に平文で保存されます。リポジトリにコミットしないこと。");
		}

		ImGui::Separator();

		if (!replayRecorder || replayRecorder->empty())
		{
			ImGui::TextDisabled("リプレイの記録がありません。Play/Pauseしてフレームを記録してください。");
			ImGui::End();
			return;
		}

		if (!isPaused)
		{
			ImGui::TextDisabled("Pause中のみ質問できます(現在のフレーム範囲を固定するため)。");
		}

		ImGui::SetNextItemWidth(160.0f);
		ImGui::SliderInt("ReplayFrames", &m_frameRangeBack, 1, 300);

		uint64_t fromFrame = (currentScrubFrame > static_cast<uint64_t>(m_frameRangeBack))
			? currentScrubFrame - static_cast<uint64_t>(m_frameRangeBack)
			: 0;
		uint64_t toFrame = currentScrubFrame;

		ImGui::Text("対象フレーム: %11u", (unsigned long long)fromFrame, (unsigned long long) toFrame);

		ImGui::Checkbox("NowFrameImage", &m_includeScreenshot);

		ImGui::BeginChild("chat_history", ImVec2(0, 300), true);
		for (auto& entry : m_history)
		{
			ImGui::TextColored(
				entry.isUser ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f) : ImVec4(0.8f, 1.0f, 0.6f, 1.0f),
				entry.isUser ? "You:" : "LLM:");
			ImGui::TextWrapped("%s", entry.text.c_str());
			ImGui::Spacing();
		}

		if (!m_lastError.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", m_lastError.c_str());
		}
		ImGui::EndChild();

		ImGui::SetNextItemWidth(-80.0f);
		bool enterPressed = ImGui::InputText(
			"##question", m_questionBuffer, sizeof(m_questionBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue
		);

		ImGui::SameLine();

		bool canSend = isPaused && !m_requestInFlight && activeScene && 
			replayRecorder && m_questionBuffer[0] != '\0';

		if (!canSend) ImGui::BeginDisabled(true);
		bool sendClicked = ImGui::Button("送信") || enterPressed;
		if (!canSend) ImGui::EndDisabled();

		if (sendClicked && canSend)
		{
			ensureClientUpToDate();

			if (EditorSettings::get().getClaudeApiKey().empty())
			{
				m_lastError = "APIキーが認定されていません。「設定」からAPIキーを入力してください";
			}
			else
			{
				const std::string question = m_questionBuffer;
				m_history.push_back({ true,question });

				auto request = m_builder.buildRequest(
					*replayRecorder, *activeScene,
					fromFrame, toFrame,
					question,
					m_includeScreenshot,
					device, context, currentRenderTarget
				);

				m_questionBuffer[0] = '\0';
				m_lastError.clear();
				m_requestInFlight = true;

				const std::string systemPrompt =
					"あなたはゲームエンジンのリプレイデータとスクリーンショットを見て、"
					"バグや意図しない挙動の原因を推測するデバッグアシスタントです。"
					"分かる範囲で簡潔に答え、断定できない場合は推測であることを明示してください。";
				auto response = m_client->sendDebugRequest(request, systemPrompt);

				m_requestInFlight = false;

				if (response.success)
				{
					m_history.push_back({ false,response.text });
				}
				else
				{
					m_lastError = response.error;
				}
			}
		}
		ImGui::End();
	}
}
