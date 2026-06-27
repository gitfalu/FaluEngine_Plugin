#include "ImGuiLayer.h"
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

}
