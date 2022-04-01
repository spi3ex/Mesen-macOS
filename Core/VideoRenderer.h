#pragma once
#include "stdafx.h"
#include "../Utilities/AutoResetEvent.h"
#include "../Utilities/IVideoRecorder.h"
#include "FrameInfo.h"

class IRenderingDevice;
class AviRecorder;
class Console;
enum class VideoCodec;

class VideoRenderer
{
private:
	shared_ptr<Console> _console;

	AutoResetEvent _waitForRender;
	IRenderingDevice* _renderer = nullptr;

	shared_ptr<IVideoRecorder> _recorder;

public:
	VideoRenderer(shared_ptr<Console> console);
	~VideoRenderer();

	void UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height);
	void RegisterRenderingDevice(IRenderingDevice *renderer);
	void UnregisterRenderingDevice(IRenderingDevice *renderer);

	void StartRecording(string filename, VideoCodec codec, uint32_t compressionLevel);
	void AddRecordingSound(int16_t* soundBuffer, uint32_t sampleCount, uint32_t sampleRate);
	void StopRecording();
	bool IsRecording();
};
