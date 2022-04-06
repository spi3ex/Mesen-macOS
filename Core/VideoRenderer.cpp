#include "stdafx.h"
#include "VideoRenderer.h"
#include "VideoDecoder.h"

void VideoRenderer::UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height)
{
	if(!_skipMode && _sendFrame) {
		//Use Blargg's NTSC filter's max size as a minimum resolution, to prevent changing resolution too often
		int32_t newWidth = std::max<int32_t>(width, NES_NTSC_OUT_WIDTH(256));
		int32_t newHeight = std::max<int32_t>(height, 240);
		if(_retroEnv != nullptr && (_previousWidth != newWidth || _previousHeight != newHeight)) {
			//Resolution change is needed
			retro_system_av_info avInfo = {};
			GetSystemAudioVideoInfo(avInfo, newWidth, newHeight);
			_retroEnv(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &avInfo);

			_previousWidth = newWidth;
			_previousHeight = newHeight;
		}

		_sendFrame(frameBuffer, width, height, sizeof(uint32_t) * width);
	}
}

void VideoRenderer::SetVideoCallback(retro_video_refresh_t sendFrame)
{
	_sendFrame = sendFrame;
}

void VideoRenderer::SetSkipMode(bool skip)
{
	_skipMode = skip;
}
