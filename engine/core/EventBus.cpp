#include "FaluEngine/EventBus.h"
// 実装はすべてヘッダーのテンプレートで完結しているため、
// このファイルは将来の非テンプレート拡張のために確保している。

namespace FaluEngine {
	EventBus& EventBus::get()
    {
        static EventBus instance;
        return instance;
    }

} // namespace FaluEngine
