#pragma once
#include <cstdint>
#include <array>
#include <glm/glm.hpp>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine {

	enum class FALU_ENGINE_API Key : uint16_t {
        // アルファベット
        A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F',
        G = 'G', H = 'H', I = 'I', J = 'J', K = 'K', L = 'L',
        M = 'M', N = 'N', O = 'O', P = 'P', Q = 'Q', R = 'R',
        S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
        Y = 'Y', Z = 'Z',

        // 数字
        Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
        Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

        // 特殊キー
        Space = 0x20,
        Escape = 0x1B,
        Enter = 0x0D,
        Tab = 0x09,
        Shift = 0x10,
        Control = 0x11,
        Alt = 0x12,

        // 方向キー
        Left = 0x25,
        Up = 0x26,
        Right = 0x27,
        Down = 0x28,

        // ファンクションキー
        F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73,
        F5 = 0x74, F6 = 0x75, F7 = 0x76, F8 = 0x77,
	};

    enum class FALU_ENGINE_API MouseButton : uint16_t {
        Left = 0,
        Right = 1,
        Middle = 2,
    };

    struct FALU_ENGINE_API KeyPressedEvent { Key key; bool repeat; };
    struct FALU_ENGINE_API KeyReleasedEvent { Key key; };
    struct FALU_ENGINE_API MouseMovedEvent { float x; float y; float dx; float dy; };
    struct FALU_ENGINE_API MouseScrolledEvent { float offsetY; };
    struct FALU_ENGINE_API MouseButtonPressedEvent { MouseButton button; };
    struct FALU_ENGINE_API MouseButtonReleasedEvent { MouseButton button; };
}
