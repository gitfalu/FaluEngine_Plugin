#include "FrameContextBuilder.h"
#include "core/Logger.h"
#include "FaluEngine/ReplayRecorder.h"
#include "renderer/dx11/ScreenshotCapture.h"
#include "renderer/dx11/RenderTexture.h"

namespace FaluEngine
{
	LLMDebugRequest FrameContextBuilder::buildRequest(ReplayRecorder& recorder, Scene& scene, uint64_t fromFrame, uint64_t toFrame, const std::string& question, bool includeCurrentScreenshot, ID3D11Device* device, ID3D11DeviceContext* context, const RenderTexture* currentRenderTarget) const
	{
		LLMDebugRequest request;

		std::string promt;
		if (!question.empty())
		{
			promt += "質問: " + question + "\n\n";
		}
		promt += recorder.describeFrameRange(scene, fromFrame, toFrame);
		request.prompt = std::move(promt);

		if (includeCurrentScreenshot)
		{
			if (!device || !context || !currentRenderTarget)
			{
				// ビルドエラーのため一度コメントアウトで対応
				// LOG_WARN("FrameContextBuilder: includeCurrentScreenshot=true device/context/RenderTargetが不足しているため、スクリーンショットは添付されません。");
			}
			else
			{
				auto png = ScreenshotCapture::captureToPNG(device, context, *currentRenderTarget);

				if (!png.empty())
				{
					LLMImageAttachment img;
					img.pngData = std::move(png);
					img.label = "current_frame";
					request.images.push_back(std::move(img));
				}
				else
				{
					LOG_WARN("FrameContextBuilder: スクリーンショットのキャプチャに失敗しました。テキストのみで続行します");
				}
			}
		}

		if (m_periodicSnapshotEnabled)
		{
			LOG_WARN("FrameContextBuilder: 定期スナップショット保存はまだ実装されていません。現在画面のみが使用されます。");
		}

		return request;
	}
}
