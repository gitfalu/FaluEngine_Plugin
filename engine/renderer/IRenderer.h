#pragma once
#include <cstdint>

namespace FaluEngine {

class Scene;

// レンダラーの抽象インターフェース。
// DirectX11 / Vulkan / OpenGL など実装を差し替えられるようにするための PAL。
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(void* windowHandle, uint32_t width, uint32_t height) = 0;
    virtual void shutdown() = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame()   = 0;

    virtual void renderScene(const Scene& scene) = 0;

    virtual void onResize(uint32_t width, uint32_t height) = 0;

    [[nodiscard]] virtual uint32_t getWidth()  const noexcept = 0;
    [[nodiscard]] virtual uint32_t getHeight() const noexcept = 0;
};

} // namespace FaluEngine
