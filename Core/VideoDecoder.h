#pragma once
#include "stdafx.h"

#include "EmulationSettings.h"
#include "FrameInfo.h"

class BaseVideoFilter;
class ScaleFilter;
class RotateFilter;
class IRenderingDevice;
class Console;
struct HdScreenInfo;

struct ScreenSize
{
	int32_t Width;
	int32_t Height;
	double Scale;
};

class VideoDecoder
{
private:
	std::shared_ptr<Console> _console;
	EmulationSettings* _settings;
	uint16_t *_ppuOutputBuffer = nullptr;
	HdScreenInfo *_hdScreenInfo = nullptr;
	bool _hdFilterEnabled = false;
	uint32_t _frameNumber = 0;

	uint32_t _frameCount = 0;

	FrameInfo _lastFrameInfo;

	VideoFilterType _videoFilterType = VideoFilterType::None;
	std::unique_ptr<BaseVideoFilter> _videoFilter;
	std::shared_ptr<ScaleFilter> _scaleFilter;
	std::shared_ptr<RotateFilter> _rotateFilter;

	void UpdateVideoFilter();
public:
	VideoDecoder(std::shared_ptr<Console> console);
	~VideoDecoder();

	void DecodeFrame();

	uint32_t GetFrameCount();

	FrameInfo GetFrameInfo();
	void GetScreenSize(ScreenSize &size, bool ignoreScale);

	void UpdateFrameSync(void* frameBuffer, HdScreenInfo *hdScreenInfo = nullptr);
	void UpdateFrame(void* frameBuffer, HdScreenInfo *hdScreenInfo = nullptr);
};
