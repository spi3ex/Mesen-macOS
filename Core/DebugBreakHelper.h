#pragma once
#include "stdafx.h"
#include "Debugger.h"
#include "Console.h"

class DebugBreakHelper
{
private:
	Debugger* _debugger;
	bool _needResume = false;

public:
	DebugBreakHelper(Debugger* debugger)
	{
		_debugger = debugger;
	}

	~DebugBreakHelper()
	{
	}
};
