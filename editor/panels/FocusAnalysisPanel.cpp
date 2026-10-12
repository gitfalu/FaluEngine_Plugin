#include "FocusAnalysisPanel.h"
#include "renderer/dx11/DX11Renderer.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Component.h"
#include "core/Logger.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>

namespace Editor
{
	namespace
	{
		constexpr const char* kFocusSettingsPath = "focus_settings.json";
	}

	void FocusAnalysisPanel::init(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		m_pass.init(device, context);
		loadSettings();
	}

	void FocusAnalysisPanel::analyzeIfNeeded(FaluEngine::DX11Renderer* renderer, FaluEngine::Scene* scene, uint32_t gameViewW, uint32_t gameViewH)
	{
		if (!renderer || !scene || !m_pass.isValid()) return;

		m_timer += ImGui::GetIO().DeltaTime;
		const bool due = m_live && m_timer >= m_interval;
		if (!due && !m_requestOnce) return;
		m_timer = 0.0f;
		m_requestOnce = false;

		const uint32_t gw = static_cast<uint32_t>(std::clamp(m_gridWidth, 64, 1920));
		const uint32_t gh = (std::max)(1u, static_cast<uint32_t>(
			static_cast<float>(gw) * static_cast<float>(gameViewH) / static_cast<float>((std::max)(1u, gameViewW)) + 0.5f));

		if (m_pass.analyze(*scene, m_settings, gw, gh, gameViewW, gameViewH, m_result))
		{
			m_pass.updatePreview(m_result, m_settings);
			m_hasResult = true;
			m_lastError.clear();
		}
		else
		{
			m_lastError = "Analysis failed (no primary camera in scene?)";
		}
		renderer->invalidateBindingCache();
	}

	void FocusAnalysisPanel::draw(FaluEngine::Scene* scene)
	{
		ImGui::Begin("Focus Analysis");
		if (!m_pass.isValid())
		{
			ImGui::TextDisabled("FocusMapPass initialization failed (see log)");
			ImGui::End();
			return;
		}

		if (ImGui::Button("Analyze Now")) m_requestOnce = true;
		ImGui::SameLine();
		ImGui::Checkbox("Live", &m_live);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat("Interval (s)", &m_interval, 0.05f, 0.1f, 5.0f, "%.2f");
		ImGui::SetNextItemWidth(160.0f);
		ImGui::SliderInt("Grid width", &m_gridWidth, 96, 960);

		if (!m_lastError.empty())
			ImGui::TextColored({ 1.0f,0.4f,0.4f,1.0f }, "%s", m_lastError.c_str());
			
		if (m_hasResult) drawResult();
		else ImGui::TextDisabled("Press 'Analyze Now' (Game View camera is used).");

		ImGui::Separator();
		drawSettings(scene);
		ImGui::End();
	}

	void FocusAnalysisPanel::drawResult()
	{
		if (auto* srv = m_pass.getPreviewSRV())
		{
			const float availW = ImGui::GetContentRegionAvail().x;
			const float aspect = static_cast<float>(m_result.height) / static_cast<float>((std::max)(1u, m_result.width));
			ImGui::Image(reinterpret_cast<ImTextureID>(srv), { availW,availW * aspect });
		}

		if (ImGui::BeginTable("##focus_cat", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("##c", ImGuiTableColumnFlags_WidthFixed, 18.0f);
			ImGui::TableSetupColumn("Category");
			ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Prio", ImGuiTableColumnFlags_WidthFixed, 36.0f);
			ImGui::TableHeadersRow();

			for (const auto& st : m_result.categoryStats)
			{
				const bool bg = st.id == 0;
				if (!bg && st.pixels == 0) continue;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const ImVec4 col = bg ? ImVec4(0.08f, 0.08f, 0.10f, 1.0f)
					: ImVec4(m_settings.categories[st.id - 1].color.r, m_settings.categories[st.id - 1].color.g,
						m_settings.categories[st.id - 1].color.b, 1.0f);
				ImGui::ColorButton(("##sw" + std::to_string(st.id)).c_str(), col,
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, { 14.0f,14.0f });
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(bg ? "(backgroud)" : m_settings.categories[st.id - 1].name.c_str());
				ImGui::TableSetColumnIndex(2);
				char overlay[32];
				std::snprintf(overlay, sizeof(overlay), "%.1f%%", st.ratio * 100.0f);
				ImGui::ProgressBar(st.ratio, { -1.0f,0.0f }, overlay);
				ImGui::TableSetColumnIndex(3);
				if (bg) ImGui::TextDisabled("-");
				else ImGui::Text("%d", m_settings.categories[st.id - 1].priority);
			}
			ImGui::EndTable();
		}
		if (ImGui::CollapsingHeader("Top entities by screen share", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int shown = 0;
			for (const auto& e : m_result.entityStats)
			{
				if (shown++ >= 12) break;
				const char* cat = (e.categoryId > 0 && e.categoryId <= static_cast<int>(m_settings.categories.size()))
					? m_settings.categories[e.categoryId - 1].name.c_str() : "?";
				ImGui::Text("%5.1f%% %s [%s]", e.ratio * 100.0f, e.name.c_str(), cat);
			}
		}
	}

	void FocusAnalysisPanel::drawSettings(FaluEngine::Scene* scene)
	{
		if (!ImGui::CollapsingHeader("Categories (color / priority)")) return;

		bool changed = false;
		ImGui::TextDisabled("Category = Inspector > Tag > category. Priority: 0 = not evaluated, 5 = most important.");

		int removeIndex = -1;
		for (size_t i = 0; i < m_settings.categories.size(); ++i)
		{
			auto& c = m_settings.categories[i];
			ImGui::PushID(static_cast<int>(i));

			changed |= ImGui::ColorEdit3("##col", &c.color.x, ImGuiColorEditFlags_NoInputs);
			ImGui::SameLine();
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%s", c.name.c_str());
			ImGui::SetNextItemWidth(130.0f);
			if (ImGui::InputText("##name", buf, sizeof(buf)))
			{
				c.name = buf;
				changed = true;
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			changed |= ImGui::SliderInt("##prio", &c.priority, 0, 5);
			ImGui::SameLine();
			if (ImGui::SmallButton("X")) removeIndex = static_cast<int>(i);

			ImGui::PopID();
		}

		if (removeIndex >= 0)
		{
			m_settings.categories.erase(m_settings.categories.begin() + removeIndex);
			changed = true;
		}

		if (ImGui::Button("Add category") && m_settings.categories.size() < 250)
		{
			m_settings.categories.push_back({ "NewCategory",{0.9f,0.9f,0.9f},0 });
			changed = true;
		}
		ImGui::SameLine();

		if (ImGui::Button("Reset to defaults"))
		{
			m_settings = FaluEngine::FocusSettings::makeDefault();
			changed = true;
		}

		if (scene)
		{
			std::set<std::string> unknown;
			for (auto e : scene->registry().view<FaluEngine::TagComponent>())
			{
				const auto& cat = scene->registry().get<FaluEngine::TagComponent>(e).category;
				if (m_settings.findId(cat) == 0) unknown.insert(cat);
			}
			for (const auto& u : unknown)
				ImGui::TextColored({ 1.0f,0.8f,0.3f,1.0f }, "Unknown category in scene: '%s' (counted as Untagged)", u.c_str());
		}

		if (changed)
		{
			saveSettings();
			if (m_hasResult) m_requestOnce = true;
		}
	}

	void FocusAnalysisPanel::loadSettings()
	{
		std::ifstream in(kFocusSettingsPath);
		if (!in.is_open()) return;
		try
		{
			nlohmann::json j;
			in >> j;
			FaluEngine::FocusSettings s;
			for (const auto& c : j.at("categories"))
			{
				FaluEngine::FocusCategory fc;
				fc.name = c.value("name", std::string("Category"));
				if (c.contains("color") && c["color"].is_array() && c["color"].size() >= 3)
					fc.color = { c["color"][0].get<float>(),c["color"][1].get<float>(),c["color"][2].get<float>() };
				fc.priority = c.value("priority", 0);
				s.categories.push_back(std::move(fc));
			}
			if (!s.categories.empty()) m_settings = std::move(s);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("FocusAnalysis: failed to parse {}: {}", kFocusSettingsPath, e.what());
		}
	}

	void FocusAnalysisPanel::saveSettings() const
	{
		nlohmann::json j;
		j["categories"] = nlohmann::json::array();
		for (const auto& c : m_settings.categories)
			j["categories"].push_back({ {"name",c.name},
				{"color",{c.color.r,c.color.g,c.color.b}},{"priority",c.priority} });

		std::ofstream out(kFocusSettingsPath);
		if (!out.is_open())
		{
			LOG_ERROR("FocusAnalysis: failed to open {} for writing", kFocusSettingsPath);
			return;
		}
		out << j.dump(4);
	}
}