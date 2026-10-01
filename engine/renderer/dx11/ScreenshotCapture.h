#pragma once
#include <vector>
#include <cstdint>
#include <FaluEngine/EngineExport.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace FaluEngine
{
	class RenderTexture;

	class FALU_ENGINE_API ScreenshotCapture
	{
	public:
		static std::vector<uint8_t> captureToPNG(
			ID3D11Device* device,
			ID3D11DeviceContext* context,
			const RenderTexture& source
		);
	};
}
