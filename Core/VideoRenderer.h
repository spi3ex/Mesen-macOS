#pragma once
#include "stdafx.h"
#include "FrameInfo.h"
#include "EmulationSettings.h"
#include "Console.h"
#include "../Utilities/nes_ntsc.h"
#include "../Libretro/libretro.h"

#define NES_PAL_FPS  (838977920.0 / 16777215.0)
#define NES_NTSC_FPS (1008307711.0 / 16777215.0)

class VideoRenderer
{
private:
	shared_ptr<Console> _console;
	retro_video_refresh_t _sendFrame = nullptr;
	retro_environment_t _retroEnv = nullptr;
	bool _skipMode = false;
	int32_t _previousHeight = -1;
	int32_t _previousWidth = -1;
public:
	VideoRenderer(shared_ptr<Console> console, retro_environment_t retroEnv)
	{
		_console = console;
		_retroEnv = retroEnv;
	}

	~VideoRenderer() { }

	void UpdateFrame(void *frameBuffer, uint32_t width, uint32_t height);
	
	void GetSystemAudioVideoInfo(retro_system_av_info &info, int32_t maxWidth = 0, int32_t maxHeight = 0)
	{
		info.timing.fps = _console->GetModel() == NesModel::NTSC ? NES_NTSC_FPS : NES_PAL_FPS;
		info.timing.sample_rate = _console->GetSettings()->GetSampleRate();

		float ratio = (float)_console->GetSettings()->GetAspectRatio(_console);
		if(ratio == 0.0f)
			ratio = (float)256 / 240;

		bool staticar = (bool)_console->GetSettings()->GetStaticAspectRatio();
		if(!staticar)
			ratio *= (float)_console->GetSettings()->GetOverscanDimensions().GetScreenWidth() / _console->GetSettings()->GetOverscanDimensions().GetScreenHeight() / 256 * 240;

		if(_console->GetSettings()->GetScreenRotation() % 180)
			info.geometry.aspect_ratio = ratio == 0.0f ? 0.0f : 1.0f / ratio;
		else
			info.geometry.aspect_ratio = ratio;

		info.geometry.base_width = _console->GetSettings()->GetOverscanDimensions().GetScreenWidth();
		info.geometry.base_height = _console->GetSettings()->GetOverscanDimensions().GetScreenHeight();

		info.geometry.max_width = maxWidth;
		info.geometry.max_height = maxHeight;

		if(maxHeight > 0 && maxWidth > 0) {
			_previousWidth = maxWidth;
			_previousHeight = maxHeight;
		}
	}

	void SetVideoCallback(retro_video_refresh_t sendFrame);
	void SetSkipMode(bool skip);
};
