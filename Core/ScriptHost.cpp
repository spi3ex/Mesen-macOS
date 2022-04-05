#include "stdafx.h"
#include "ScriptHost.h"
#include "ScriptingContext.h"

#ifndef LIBRETRO
#include "LuaScriptingContext.h"
#endif

ScriptHost::ScriptHost(int scriptId)
{
	_scriptId = scriptId;
}

int ScriptHost::GetScriptId()
{
	return _scriptId;
}

bool ScriptHost::LoadScript(string scriptName, string scriptContent, Debugger* debugger)
{
#ifndef LIBRETRO
	_context.reset(new LuaScriptingContext(debugger));
	if(!_context->LoadScript(scriptName, scriptContent, debugger)) {
		return false;
	}
	return true;
#else
	return false;
#endif
}

void ScriptHost::ProcessEvent(EventType eventType)
{
	if(_context) {
		_context->CallEventCallback(eventType);
	}
}

bool ScriptHost::ProcessSavestate()
{
	if(_context) {
		return _context->ProcessSavestate();
	}
	return false;
}

bool ScriptHost::CheckStateLoadedFlag()
{
	if(_context) {
		return _context->CheckStateLoadedFlag();
	}
	return false;
}
