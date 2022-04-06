#include "stdafx.h"
#include "BaseVideoFilter.h"
#include "StandardController.h"
#include "ScaleFilter.h"
#include "RotateFilter.h"
#include "Console.h"

BaseVideoFilter::BaseVideoFilter(shared_ptr<Console> console)
{
	_console = console;
	_overscan = _console->GetSettings()->GetOverscanDimensions();
}

BaseVideoFilter::~BaseVideoFilter()
{
	auto lock = _frameLock.AcquireSafe();
	if(_outputBuffer) {
		delete[] _outputBuffer;
		_outputBuffer = nullptr;
	}
}

void BaseVideoFilter::UpdateBufferSize()
{
	uint32_t newBufferSize = GetFrameInfo().Width*GetFrameInfo().Height;
	if(_bufferSize != newBufferSize) {
		_frameLock.Acquire();
		if(_outputBuffer) {
			delete[] _outputBuffer;
		}

		_bufferSize = newBufferSize;
		_outputBuffer = new uint32_t[newBufferSize];
		_frameLock.Release();
	}
}

OverscanDimensions BaseVideoFilter::GetOverscan()
{
	return _overscan;
}

void BaseVideoFilter::OnBeforeApplyFilter()
{
}

bool BaseVideoFilter::IsOddFrame()
{
	return _isOddFrame;
}

void BaseVideoFilter::SendFrame(uint16_t *ppuOutputBuffer, uint32_t frameNumber)
{
	_frameLock.Acquire();
	_overscan = _console->GetSettings()->GetOverscanDimensions();
	_isOddFrame = frameNumber % 2;
	UpdateBufferSize();
	OnBeforeApplyFilter();
	ApplyFilter(ppuOutputBuffer);

	_frameLock.Release();
}

uint32_t* BaseVideoFilter::GetOutputBuffer()
{
	return _outputBuffer;
}
