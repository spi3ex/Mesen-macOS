#pragma once

#include "stdafx.h"

#include "CPU.h"
#include "DebuggerTypes.h"

class CPU;
class APU;
class PPU;
class Console;
class BaseMapper;

class Debugger
{
private:
	shared_ptr<Console> _console;
	shared_ptr<CPU> _cpu;
	shared_ptr<PPU> _ppu;
	shared_ptr<APU> _apu;
	shared_ptr<BaseMapper> _mapper;
	
	uint32_t _flags;
	
public:
	Debugger(shared_ptr<Console> console, shared_ptr<CPU> cpu, shared_ptr<PPU> ppu, shared_ptr<APU> apu, shared_ptr<MemoryManager> memoryManager, shared_ptr<BaseMapper> mapper);
	~Debugger();

	void SetPpu(shared_ptr<PPU> ppu);
	Console* GetConsole();

	void GetState(DebugState *state, bool includeMapperInfo = true);
	void SetState(DebugState state);
};
