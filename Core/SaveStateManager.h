#pragma once
#include "stdafx.h"
#include <memory>

class Console;

class SaveStateManager
{
private:
	std::shared_ptr<Console> _console;
public:
	static constexpr uint32_t FileFormatVersion = 13;

	SaveStateManager(std::shared_ptr<Console> console);

	void GetSaveStateHeader(ostream & stream);

	void SaveState(ostream &stream);
	bool LoadState(istream &stream, bool hashCheckRequired = true);
};
