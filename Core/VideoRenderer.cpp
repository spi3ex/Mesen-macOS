#include "stdafx.h"
#include "IRenderingDevice.h"
#include "VideoRenderer.h"
#include "VideoDecoder.h"
#include "Console.h"
#include "../Utilities/IVideoRecorder.h"

VideoRenderer::VideoRenderer(shared_ptr<Console> console)
{
	_console = console;
}

VideoRenderer::~VideoRenderer()
{
}

void VideoRenderer::UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height)
{
	if(_renderer) {		
		_renderer->UpdateFrame(frameBuffer, width, height);
		_waitForRender.Signal();
	}
}

void VideoRenderer::RegisterRenderingDevice(IRenderingDevice *renderer)
{
	_renderer = renderer;
}

void VideoRenderer::UnregisterRenderingDevice(IRenderingDevice *renderer)
{
	if(_renderer == renderer)
		_renderer = nullptr;
}

void VideoRenderer::StartRecording(string filename, VideoCodec codec, uint32_t compressionLevel)
{
}

void VideoRenderer::AddRecordingSound(int16_t* soundBuffer, uint32_t sampleCount, uint32_t sampleRate)
{
}

void VideoRenderer::StopRecording()
{
}

bool VideoRenderer::IsRecording()
{
	return false;
}
