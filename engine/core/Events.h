#pragma once
#include <cstdint>

namespace FaluEngine
{

	//======= ウィンドウ系イベント =======
	struct WindowResizeEvent {
		uint32_t width;
		uint32_t height;
	};

	struct WindowCloseEvent {};

	struct WindowFocusEvent {
		bool focused; // true : フォーカス取得 , false : フォーカス喪失
	};

	//======= アプリ系イベント ========
	struct AppUpdateEvent {
		float deltaTime;
	};

	struct AppRenderEvent {};

	
}// namespace FaluEngine
