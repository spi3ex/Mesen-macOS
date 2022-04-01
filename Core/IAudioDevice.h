#pragma once

#include "stdafx.h"

class IAudioDevice
{
	public:
		virtual ~IAudioDevice() {}
		virtual void PlayBuffer(int16_t *soundBuffer, uint32_t bufferSize, uint32_t sampleRate, bool isStereo) = 0;
};
