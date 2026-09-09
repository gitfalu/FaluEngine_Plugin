#include "FaluEngine/ImGuiLayer.h"
#include "core/Logger.h"


#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <ImGuizmo.h>

#include "core/PathResolver.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace FaluEngine{

	bool ImGuiLayer::init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();

		ApplyModernEngineStyle();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		auto font = io.Fonts->AddFontFromFileTTF(
			PathResolver::resolveStr("assets/fonts/CascadiaCode.ttf").c_str(),
			15.0f,
			NULL,
			io.Fonts->GetGlyphRangesJapanese());

		IM_ASSERT(font != nullptr);
		

		if (!ImGui_ImplWin32_Init(hwnd)) {
			LOG_ERROR("ImGui_ImplWin32_Init failed");
			return false;
		}

		if (!ImGui_ImplDX11_Init(device, context)) {
			LOG_ERROR("ImGui_ImplDX11_Init failed");
			return false;
		}

		m_initialized = true;
		LOG_INFO("ImGuiLayer initialized");

		return true;
	}


	void ImGuiLayer::shutdown()
	{
		if (!m_initialized) return;
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		m_initialized = false;
		LOG_INFO("ImGuiLayer shutdown");
	}


	void ImGuiLayer::begin()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::end()
	{
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	bool ImGuiLayer::handleWin32Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0;
	}


	void* ImGuiLayer::getContext() const noexcept
	{
		return ImGui::GetCurrentContext();
	}

	void ImGuiLayer::ApplyModernEngineStyle() {
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		// --- 形状・余白の設定 ---
		style.WindowRounding = 4.0f;  // ウィンドウ角の丸み
		style.ChildRounding = 4.0f;  // 子ウィンドウの丸み
		style.FrameRounding = 3.0f;  // 入力フォーム・ボタンの丸み
		style.PopupRounding = 4.0f;  // ポップアップの丸み
		style.ScrollbarRounding = 9.0f;  // スクロールバーの丸み
		style.GrabRounding = 3.0f;  // スライダーつまみの丸み

		style.WindowBorderSize = 1.0f;  // ウインドウ境界線の太さ
		style.FrameBorderSize = 0.0f;  // フレーム境界線
		style.PopupBorderSize = 1.0f;  // ポップアップの境界線

		style.WindowPadding = ImVec2(10.0f, 10.0f); // 内側の余白
		style.FramePadding = ImVec2(6.0f, 4.0f);   // フォーム内の余白
		style.ItemSpacing = ImVec2(8.0f, 6.0f);   // 要素同士の間隔
		style.ScrollbarSize = 12.0f;                // スクロールバーの幅

		// --- カラーパレット設定（深みのあるダークグレー + アクセント） ---
		// 背景・ベースカラー
		colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.15f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);

		// タイトルバー（視認性を上げるための僅かなグラデーション感）
		colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);

		// テキスト
		colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.93f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);

		// フレーム・入力項目（テキストボックス・チェックボックス等）
		colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.27f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);

		// ボタン
		colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.34f, 0.34f, 0.38f, 1.00f);

		// アクセントカラー（アクセントブルー／UE5・Unity風）
		colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.38f, 0.55f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.32f, 0.48f, 1.00f);

		// タブ（Docking使用時）
		colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.38f, 0.55f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);

		// スライダー・チェックボックス等のツマミ
		colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.48f, 0.68f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.42f, 0.58f, 0.82f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.42f, 0.58f, 0.82f, 1.00f);

		// スクロールバー
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.42f, 1.00f);
	}
}
