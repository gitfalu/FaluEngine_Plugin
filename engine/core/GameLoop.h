#pragma once
#include <functional>

namespace FaluEngine {

// コールバックが false を返すとループを抜ける
using LoopCallback = std::function<bool(float deltaTime)>;

class GameLoop {
public:
    // 固定ステップ (fixedDt 秒) + 可変レンダーステップのハイブリッド方式
    void run(LoopCallback callback, double fixedDt = 1.0 / 60.0);

    [[nodiscard]] double getFixedDt()   const noexcept { return m_fixedDt; }
    [[nodiscard]] uint64_t getFrameNo() const noexcept { return m_frameNo; }

private:
    double   m_fixedDt = 1.0 / 60.0;
    uint64_t m_frameNo = 0;
};

} // namespace FaluEngine
