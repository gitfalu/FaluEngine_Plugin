#pragma once

#include <string>
#include <cstdint>
#include "FaluEngine/FocusTypes.h"
#include "renderer/dx11/FocusMapPass.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace FaluEngine
{
	class Scene;
	class DX11Renderer;
}

namespace Editor
{
	class FocusAnalysisPanel
	{
	public:
		void init(ID3D11Device* device, ID3D11DeviceContext* context);

		void analyzeIfNeeded(FaluEngine::DX11Renderer* renderer, FaluEngine::Scene* scene,
			uint32_t gameViewW, uint32_t gameViewH);

		void draw(FaluEngine::Scene* scene);

		[[nodiscard]] const FaluEngine::FocusSettings& getSettings() const noexcept { return m_settings; }
		[[nodiscard]] const FaluEngine::FocusMapResult& getResult() const noexcept { return m_result; }
		[[nodiscard]] bool hasResult() const noexcept { return m_hasResult; }
	private:
		void loadSettings();
		void saveSettings() const;
		void drawResult();
		void drawSettings(FaluEngine::Scene* scene);

		FaluEngine::FocusMapPass m_pass;
		FaluEngine::FocusSettings m_settings = FaluEngine::FocusSettings::makeDefault();
		FaluEngine::FocusMapResult m_result;
		bool m_hasResult = false;

		bool m_live = false;
		bool m_requestOnce = false;
		float m_interval = 0.5f;
		float m_timer = 0.0f;
		int m_gridWidth = 480;
		std::string m_lastError;

	};
}