#include "stdafx.h"
#include "Debugger.h"
#include "Console.h"
#include "BaseMapper.h"
#include "APU.h"
#include "PPU.h"
#include "StandardController.h"

Debugger::Debugger(shared_ptr<Console> console, shared_ptr<CPU> cpu, shared_ptr<PPU> ppu, shared_ptr<APU> apu, shared_ptr<MemoryManager> memoryManager, shared_ptr<BaseMapper> mapper)
{
	_console = console;
	_cpu = cpu;
	_apu = apu;
	_mapper = mapper;

	SetPpu(ppu);

	_flags = 0;
}

Debugger::~Debugger()
{
}

void Debugger::SetPpu(shared_ptr<PPU> ppu)
{
	_ppu = ppu;
}

Console* Debugger::GetConsole()
{
	return _console.get();
}

void Debugger::GetState(DebugState *state, bool includeMapperInfo)
{
	state->Model = _console->GetModel();
	state->ClockRate = _cpu->GetClockRate(_console->GetModel());
	_cpu->GetState(state->CPU);
	_ppu->GetState(state->PPU);
	if(includeMapperInfo) {
		state->Cartridge = _mapper->GetState();
		state->APU = _apu->GetState();
	}
}

void Debugger::SetState(DebugState state)
{
	_cpu->SetState(state.CPU);
	_ppu->SetState(state.PPU);
}
