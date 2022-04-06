#pragma once

#include "stdafx.h"
#include <atomic>
#include <unordered_set>
using std::atomic;
using std::unordered_set;

#include "Cpu.h"
#include "DebuggerTypes.h"

class CPU;
class APU;
class PPU;
class MemoryManager;
class Console;
class LabelManager;
class BaseMapper;
class TraceLogger;
class CodeDataLogger;
class DummyCpu;
struct ExpressionData;

enum EvalResultType : int32_t;

class Debugger
{
private:
	shared_ptr<Console> _console;
	shared_ptr<CPU> _cpu;
	shared_ptr<PPU> _ppu;
	shared_ptr<APU> _apu;
	shared_ptr<MemoryManager> _memoryManager;
	shared_ptr<BaseMapper> _mapper;
	
	uint32_t _opCodeCycle;
	MemoryOperationType _memoryOperationType;

	unordered_set<uint32_t> _functionEntryPoints;

	//Used to alter the executing address via "Set Next Statement"
	uint16_t *_currentReadAddr;
	uint8_t *_currentReadValue;
	int32_t _nextReadAddr;
	uint16_t _returnToAddress;

	uint32_t _flags;

	string _romName;
	
	atomic<bool> _released;

	int64_t _prevInstructionCycle;
	int64_t _curInstructionCycle;
	
	uint32_t _inputOverride[4];

public:
	Debugger(shared_ptr<Console> console, shared_ptr<CPU> cpu, shared_ptr<PPU> ppu, shared_ptr<APU> apu, shared_ptr<MemoryManager> memoryManager, shared_ptr<BaseMapper> mapper);
	~Debugger();

	void SetPpu(shared_ptr<PPU> ppu);
	Console* GetConsole();

	void SetFlags(uint32_t flags);
	bool CheckFlag(DebuggerFlags flag);
	
	void GetApuState(ApuState *state);
	void GetState(DebugState *state, bool includeMapperInfo = true);
	void SetState(DebugState state);

	void SetNextStatement(uint16_t addr);

	int32_t GetRelativeAddress(uint32_t addr, AddressType type);
	int32_t GetRelativePpuAddress(uint32_t addr, PpuAddressType type);
	int32_t GetAbsoluteAddress(uint32_t addr);	
	int32_t GetAbsoluteChrAddress(uint32_t addr);
	
	void GetAbsoluteAddressAndType(uint32_t relativeAddr, AddressTypeInfo* info);
	void GetPpuAbsoluteAddressAndType(uint32_t relativeAddr, PpuAddressTypeInfo* info);

	void GetNesHeader(uint8_t* header);
};
