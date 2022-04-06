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

void Debugger::SetFlags(uint32_t flags)
{
	_flags = flags;
}

bool Debugger::CheckFlag(DebuggerFlags flag)
{
	return (_flags & (uint32_t)flag) == (uint32_t)flag;
}

void Debugger::GetApuState(ApuState *state)
{
	//Force APU to catch up before we retrieve its state
	_apu->Run();

	*state = _apu->GetState();
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

int32_t Debugger::GetRelativeAddress(uint32_t addr, AddressType type)
{
	switch(type) {
		case AddressType::InternalRam: 
		case AddressType::Register:
			return addr;
		
		case AddressType::PrgRom: 
		case AddressType::WorkRam: 
		case AddressType::SaveRam: 
			return _mapper->FromAbsoluteAddress(addr, type);
	}

	return -1;
}

int32_t Debugger::GetRelativePpuAddress(uint32_t addr, PpuAddressType type)
{
	if(type == PpuAddressType::PaletteRam) {
		return 0x3F00 | (addr & 0x1F);
	}
	return _mapper->FromAbsolutePpuAddress(addr, type);
}

int32_t Debugger::GetAbsoluteAddress(uint32_t addr)
{
	return _mapper->ToAbsoluteAddress(addr);
}

int32_t Debugger::GetAbsoluteChrAddress(uint32_t addr)
{
	return _mapper->ToAbsoluteChrAddress(addr);
}

void Debugger::GetAbsoluteAddressAndType(uint32_t relativeAddr, AddressTypeInfo* info)
{
	return _mapper->GetAbsoluteAddressAndType(relativeAddr, info);
}

void Debugger::GetPpuAbsoluteAddressAndType(uint32_t relativeAddr, PpuAddressTypeInfo* info)
{
	return _mapper->GetPpuAbsoluteAddressAndType(relativeAddr, info);
}
