#pragma once 

#ifdef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d11.h>

namespace FaluEngine {
	
class ImGuiLayer {
public:
	bool init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

	void shutdown();

	void begin();

	void end();

	static bool handleWin32Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

	[[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
private:
	bool m_initialized = false;
};

}

