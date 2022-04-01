#pragma once
#include "stdafx.h"
#include "Console.h"

class ConsolePauseHelper
{
private:
	Console* _console;

public:
	ConsolePauseHelper(Console* console)
	{
		_console = console;
		//pause at the end of the next frame
		console->Pause();
	}

	~ConsolePauseHelper()
	{
		_console->Resume();
	}
};
