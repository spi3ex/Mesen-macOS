#pragma once
#include "stdafx.h"
#include "FrameInfo.h"

class IRenderingDevice;
class Console;

class VideoRenderer
{
private:
	IRenderingDevice* _renderer = nullptr;

public:
	VideoRenderer(shared_ptr<Console> console);
	~VideoRenderer();

	void UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height);
	void RegisterRenderingDevice(IRenderingDevice *renderer);
	void UnregisterRenderingDevice(IRenderingDevice *renderer);
};
