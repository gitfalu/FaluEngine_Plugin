#include "ScreenshotCapture.h"
#include "RenderTexture.h"
#include "core/Logger.h"

#include <DirectXTex.h>

namespace FaluEngine
{
	std::vector<uint8_t> ScreenshotCapture::captureToPNG(ID3D11Device* device, ID3D11DeviceContext* context, const RenderTexture& source)
	{
		if (!device || !context || !source.isValid())
		{
			LOG_ERROR("ScreenshotCapture: invalid device/context/source.");
			return {};
		}

		DirectX::ScratchImage image;
		HRESULT hr = DirectX::CaptureTexture(
			device, context, source.getTexture(), image
		);

		if (FAILED(hr))
		{
			LOG_ERROR("ScreenshotCapture: CaptureTexture failed (hr=0x{:08X}).", static_cast<uint32_t>(hr));
			return {};
		}

		DirectX::Blob blob;
		hr = DirectX::SaveToWICMemory(
			*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE,
			DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
			blob
		);

		if (FAILED(hr))
		{
			LOG_ERROR("ScreenshotCapture: SaveToWICMemory (PNG) failed (hr=0x{:08X})", static_cast<uint32_t>(hr));
			return {};
		}

		const uint8_t* data = static_cast<const uint8_t*>(blob.GetBufferPointer());
		return std::vector<uint8_t>(data,data + blob.GetBufferSize());
	}
}
