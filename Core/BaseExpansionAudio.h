#pragma once
#include "stdafx.h"
#include "Snapshotable.h"
#include "EmulationSettings.h"

class MemoryManager;

class BaseExpansionAudio : public Snapshotable
{
protected: 
	std::shared_ptr<Console> _console = nullptr;

	virtual void ClockAudio() = 0;
	void StreamState(bool saving) override;

public:
	BaseExpansionAudio(std::shared_ptr<Console> console);

	void Clock();
};
