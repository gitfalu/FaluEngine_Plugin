#include "Win32Platform.h"
#include "core/Logger.h"
#include "renderer/dx11/ImGuiLayer.h"
#include "core/InputManager.h"

namespace FaluEngine {

bool Win32Window::create(const Desc& desc) {
    m_width  = desc.width;
    m_height = desc.height;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FaluEngineWnd";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(
        0, L"FaluEngineWnd", desc.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) {
        LOG_ERROR("CreateWindowExW failed");
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    LOG_INFO("Win32Window created ({}x{})", m_width, m_height);
    return true;
}

void Win32Window::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool Win32Window::pollEvents() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK Win32Window::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (FaluEngine::ImGuiLayer::handleWin32Message(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
            
    case WM_SIZE:
        if (self && wp != SIZE_MINIMIZED) {
            self->m_width  = LOWORD(lp);
            self->m_height = HIWORD(lp);
            if (self->m_onResize) self->m_onResize(self->m_width, self->m_height);
        }
        return 0;
    case WM_KEYDOWN:
        InputManager::get().onKeyDown(
            static_cast<uint16_t>(wp), (lp & 0x40000000) != 0);
        return 0;
    case WM_KEYUP:
        InputManager::get().onKeyup(static_cast<uint16_t>(wp));
        return 0;
    case WM_MOUSEMOVE:
        InputManager::get().onMouseMove(
            static_cast<float>(LOWORD(lp)),
            static_cast<float>(HIWORD(lp))
        );
        return 0;
    case WM_LBUTTONDOWN:
        InputManager::get().onMouseButtonDown(MouseButton::Left); return 0;
    case WM_LBUTTONUP:
        InputManager::get().onMouseButtonUp(MouseButton::Left); return 0;
    case WM_RBUTTONDOWN:
        InputManager::get().onMouseButtonDown(MouseButton::Right); return 0;
    case WM_RBUTTONUP:
        InputManager::get().onMouseButtonUp(MouseButton::Right); return 0;
    case WM_MBUTTONDOWN:
        InputManager::get().onMouseButtonDown(MouseButton::Middle); return 0;
    case WM_MBUTTONUP:
        InputManager::get().onMouseButtonUp(MouseButton::Middle); return 0;

    case WM_MOUSEWHEEL:
        InputManager::get().onMouseScroll(
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA);
        return 0;
    case WM_DESTROY:
        if (self && self->m_onClose) self->m_onClose();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace FaluEngine
