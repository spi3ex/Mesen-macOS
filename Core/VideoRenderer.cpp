#include "stdafx.h"
#include "IRenderingDevice.h"
#include "VideoRenderer.h"
#include "VideoDecoder.h"

VideoRenderer::VideoRenderer()
{
}

VideoRenderer::~VideoRenderer()
{
}

void VideoRenderer::UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height)
{
	if(_renderer)
		_renderer->UpdateFrame(frameBuffer, width, height);
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
