#pragma once
#include "stdafx.h"
#include "DebuggerTypes.h"

class ScriptingContext;
class Debugger;

class ScriptHost
{
private:
	shared_ptr<ScriptingContext> _context;
	int _scriptId;

public:
	ScriptHost(int scriptId);

	int GetScriptId();

	bool LoadScript(string scriptName, string scriptContent, Debugger* debugger);

	void ProcessEvent(EventType eventType);
	bool ProcessSavestate();

	bool CheckStateLoadedFlag();
};
