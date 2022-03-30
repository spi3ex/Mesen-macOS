#pragma once
#include "stdafx.h"
#include "../Core/IAudioDevice.h"
#include "../Core/SoundMixer.h"
#include "libretro.h"

class LibretroSoundManager : public IAudioDevice
{
private:
	retro_audio_sample_batch_t audio_batch_cb = nullptr;
	bool _skipMode = false;

public:
	LibretroSoundManager(shared_ptr<Console> console)
	{
	}

	~LibretroSoundManager()
	{
	}

	// Inherited via IAudioDevice
	virtual void PlayBuffer(int16_t *soundBuffer, uint32_t sampleCount, uint32_t sampleRate, bool isStereo) override
	{
		if(!_skipMode && audio_batch_cb) {
			for(uint32_t total = 0; total < sampleCount; ) {
				total += (uint32_t)audio_batch_cb(soundBuffer + total*2, sampleCount - total);
			}
		}
	}

	void SetSendAudioSample(retro_audio_sample_batch_t sendAudioSample)
	{
		audio_batch_cb = sendAudioSample;
	}

	void SetSkipMode(bool skip)
	{
		_skipMode = skip;
	}

	virtual void Stop() override
	{
	}

	virtual void Pause() override
	{
	}

	virtual void UpdateSoundSettings() override
	{
	}

	virtual void ProcessEndOfFrame() override
	{
	}
};
