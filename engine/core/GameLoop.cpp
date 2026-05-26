#include "GameLoop.h"
#include <chrono>
#include <algorithm>

namespace FaluEngine {

void GameLoop::run(LoopCallback callback, double fixedDt) {
    m_fixedDt = fixedDt;

    using Clock     = std::chrono::steady_clock;
    using Duration  = std::chrono::duration<double>;

    auto prev      = Clock::now();
    double accumulator = 0.0;

    while (true) {
        auto  now     = Clock::now();
        double elapsed = std::chrono::duration_cast<Duration>(now - prev).count();
        prev = now;

        // スパイク対策: 最大フレーム時間を 0.25 秒に制限
        elapsed = std::min(elapsed, 0.25);
        accumulator += elapsed;

        // 固定ステップ更新
        while (accumulator >= m_fixedDt) {
            if (!callback(static_cast<float>(m_fixedDt))) return;
            accumulator -= m_fixedDt;
            ++m_frameNo;
        }
    }
}

} // namespace FaluEngine
