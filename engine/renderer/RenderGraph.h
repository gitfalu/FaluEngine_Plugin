#pragma once
#include <FaluEngine/EngineExport.h>

// RenderGraph は将来のフレームグラフ実装のために予約している。
// 現状は DX11Renderer が直接 RTV / DSV を管理する。
namespace FaluEngine {
class FALU_ENGINE_API RenderGraph {};
} // namespace FaluEngine
