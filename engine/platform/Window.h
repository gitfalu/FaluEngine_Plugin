#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace FaluEngine {

// ウィンドウイベントのコールバック型
using ResizeCallback = std::function<void(uint32_t w, uint32_t h)>;
using CloseCallback  = std::function<void()>;

class Window {
public:
    struct Desc {
        std::wstring title  = L"FaluEngine";
        uint32_t       width  = 1280;
        uint32_t       height = 720;
    };

    virtual ~Window() = default;

    virtual bool create(const Desc& desc) = 0;
    virtual void destroy()                = 0;

    // メッセージポンプ: false を返すとアプリ終了
    virtual bool pollEvents() = 0;

    virtual void setResizeCallback(ResizeCallback cb) = 0;
    virtual void setCloseCallback(CloseCallback  cb)  = 0;
    virtual void setTitle(const std::wstring& title) = 0;

    [[nodiscard]] virtual void*    getNativeHandle() const noexcept = 0;
    [[nodiscard]] virtual uint32_t getWidth()  const noexcept = 0;
    [[nodiscard]] virtual uint32_t getHeight() const noexcept = 0;
};

} // namespace FaluEngine
