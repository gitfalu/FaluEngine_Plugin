#pragma once

// Windows.h を取り込む前に必ずガードする
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "platform/Window.h"

namespace FaluEngine {

class Win32Window final : public Window {
public:
    Win32Window()  = default;
    ~Win32Window() override { destroy(); }

    bool create(const Desc& desc) override;
    void destroy() override;
    bool pollEvents() override;

    void setResizeCallback(ResizeCallback cb) override { m_onResize = std::move(cb); }
    void setCloseCallback(CloseCallback  cb)  override { m_onClose  = std::move(cb); }

    [[nodiscard]] void*    getNativeHandle() const noexcept override { return m_hwnd; }
    [[nodiscard]] uint32_t getWidth()  const noexcept override { return m_width; }
    [[nodiscard]] uint32_t getHeight() const noexcept override { return m_height; }

    void setTitle(const std::wstring& title) override 
    {
        SetWindowTextW(m_hwnd, title.c_str());
    }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND     m_hwnd   = nullptr;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;

    ResizeCallback m_onResize;
    CloseCallback  m_onClose;
};

} // namespace FaluEngine
