#include "stdafx.h"
#include "VideoDecoder.h"
#include "EmulationSettings.h"
#include "DefaultVideoFilter.h"
#include "RawVideoFilter.h"
#include "BisqwitNtscFilter.h"
#include "NtscFilter.h"
#include "HdVideoFilter.h"
#include "ScaleFilter.h"
#include "VideoRenderer.h"
#include "Console.h"
#include "PPU.h"
#include "HdData.h"
#include "HdNesPack.h"
#include "RotateFilter.h"

VideoDecoder::VideoDecoder(shared_ptr<Console> console)
{
	_console = console;
	_settings = _console->GetSettings();
	UpdateVideoFilter();
}

VideoDecoder::~VideoDecoder()
{
}

FrameInfo VideoDecoder::GetFrameInfo()
{
	return _lastFrameInfo;
}

void VideoDecoder::GetScreenSize(ScreenSize &size, bool ignoreScale)
{
	if(_videoFilter) {
		OverscanDimensions overscan = ignoreScale ? _videoFilter->GetOverscan() : _console->GetSettings()->GetOverscanDimensions();
		FrameInfo frameInfo{ overscan.GetScreenWidth(), overscan.GetScreenHeight(), PPU::ScreenWidth, PPU::ScreenHeight, 4 };
		double aspectRatio = _console->GetSettings()->GetAspectRatio(_console);
		double scale = (ignoreScale ? 1 : _console->GetSettings()->GetVideoScale());
		size.Width = (int32_t)(frameInfo.Width * scale);
		size.Height = (int32_t)(frameInfo.Height * scale);
		if(aspectRatio != 0.0) {
			size.Width = (uint32_t)(frameInfo.OriginalHeight * scale * aspectRatio * ((double)frameInfo.Width / frameInfo.OriginalWidth));
		}

		if(_console->GetSettings()->GetScreenRotation() % 180) {
			std::swap(size.Width, size.Height);
		}

		size.Scale = scale;
	}
}

void VideoDecoder::UpdateVideoFilter()
{
	VideoFilterType newFilter = _console->GetSettings()->GetVideoFilterType();

	if(_videoFilterType != newFilter || _videoFilter == nullptr || (_hdScreenInfo && !_hdFilterEnabled) || (!_hdScreenInfo && _hdFilterEnabled)) {
		_videoFilterType = newFilter;
		_videoFilter.reset(new DefaultVideoFilter(_console));
		_scaleFilter.reset();

		switch(_videoFilterType) {
			case VideoFilterType::None: break;
			case VideoFilterType::NTSC: _videoFilter.reset(new NtscFilter(_console)); break;
			case VideoFilterType::BisqwitNtsc: _videoFilter.reset(new BisqwitNtscFilter(_console, 1)); break;
			case VideoFilterType::BisqwitNtscHalfRes: _videoFilter.reset(new BisqwitNtscFilter(_console, 2)); break;
			case VideoFilterType::BisqwitNtscQuarterRes: _videoFilter.reset(new BisqwitNtscFilter(_console, 4)); break;
			case VideoFilterType::Raw: _videoFilter.reset(new RawVideoFilter(_console)); break;
			default: _scaleFilter = ScaleFilter::GetScaleFilter(_videoFilterType); break;
		}

		_hdFilterEnabled = false;
		if(_hdScreenInfo) {
			_videoFilter.reset(new HdVideoFilter(_console, _console->GetHdData()));
			_hdFilterEnabled = true;
		}
	}

	if(_console->GetSettings()->GetScreenRotation() == 0 && _rotateFilter) {
		_rotateFilter.reset();
	} else if(_console->GetSettings()->GetScreenRotation() > 0) {
		if(!_rotateFilter || _rotateFilter->GetAngle() != _console->GetSettings()->GetScreenRotation()) {
			_rotateFilter.reset(new RotateFilter(_console->GetSettings()->GetScreenRotation()));
		}
	}
}

void VideoDecoder::DecodeFrame()
{
	UpdateVideoFilter();

	if(_hdFilterEnabled) {
		((HdVideoFilter*)_videoFilter.get())->SetHdScreenTiles(_hdScreenInfo);
	}
	_videoFilter->SendFrame(_ppuOutputBuffer, _frameNumber);

	uint32_t* outputBuffer = _videoFilter->GetOutputBuffer();
	FrameInfo frameInfo = _videoFilter->GetFrameInfo();

	if(_rotateFilter) {
		outputBuffer = _rotateFilter->ApplyFilter(outputBuffer, frameInfo.Width, frameInfo.Height);
		frameInfo = _rotateFilter->GetFrameInfo(frameInfo);
	}

	if(_scaleFilter) {
		outputBuffer = _scaleFilter->ApplyFilter(outputBuffer, frameInfo.Width, frameInfo.Height, _console->GetSettings()->GetPictureSettings().ScanlineIntensity);
		frameInfo = _scaleFilter->GetFrameInfo(frameInfo);
	}

	ScreenSize screenSize;
	GetScreenSize(screenSize, true);
	
	_lastFrameInfo = frameInfo;

	_console->GetVideoRenderer()->UpdateFrame(outputBuffer, frameInfo.Width, frameInfo.Height);
}

uint32_t VideoDecoder::GetFrameCount()
{
	return _frameCount;
}

void VideoDecoder::UpdateFrameSync(void *ppuOutputBuffer, HdScreenInfo *hdScreenInfo)
{
	_frameNumber = _console->GetFrameCount();
	_hdScreenInfo = hdScreenInfo;
	_ppuOutputBuffer = (uint16_t*)ppuOutputBuffer;
	DecodeFrame();
	_frameCount++;
}

void VideoDecoder::UpdateFrame(void *ppuOutputBuffer, HdScreenInfo *hdScreenInfo)
{
	_frameNumber = _console->GetFrameCount();
	_hdScreenInfo = hdScreenInfo;
	_ppuOutputBuffer = (uint16_t*)ppuOutputBuffer;

	_frameCount++;
}
