#include "stdafx.h"
#include "ScriptHost.h"
#include "ScriptingContext.h"

ScriptHost::ScriptHost(int scriptId)
{
	_scriptId = scriptId;
}

int ScriptHost::GetScriptId()
{
	return _scriptId;
}

bool ScriptHost::CheckStateLoadedFlag()
{
	if(_context) {
		return _context->CheckStateLoadedFlag();
	}
	return false;
}
