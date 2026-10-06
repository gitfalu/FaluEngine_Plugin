#pragma once

#include <FaluEngine/LLMClient.h>
#include <FaluEngine/EngineExport.h>
#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace FaluEngine
{
	class Scene;
	class ReplayRecorder;
	class RenderTexture;

	class FALU_ENGINE_API FrameContextBuilder
	{
	public:
		/// @brief 定期スナップショット設定の変更
		/// @param enabled 
		void setPeriodicSnapshotEnabled(bool enabled) noexcept { m_periodicSnapshotEnabled = enabled; }

		/// @brief 定期スナップショットの有無
		/// @return 
		[[nodiscard]] bool isPeriodicSnapshotEnabled() const noexcept { return m_periodicSnapshotEnabled; }

		/// @brief 現在の画面からからリクエストを組み立てる
		/// @param recorder 
		/// @param scene 
		/// @param fromFrame 
		/// @param toFrame 
		/// @param question 
		/// @param includeCurrentScreenshot 
		/// @param device 
		/// @param context 
		/// @param currentRenderTarget 
		/// @return 
		[[nodiscard]] LLMDebugRequest buildRequest(
			ReplayRecorder& recorder,
			Scene& scene,
			uint64_t fromFrame,
			uint64_t toFrame,
			const std::string& question,
			bool includeCurrentScreenshot,
			ID3D11Device* device = nullptr,
			ID3D11DeviceContext* context = nullptr,
			const RenderTexture* currentRenderTarget = nullptr
		) const;

	private:
		bool m_periodicSnapshotEnabled = false;
	};
}
