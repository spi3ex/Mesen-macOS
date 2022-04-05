#pragma once

#include "stdafx.h"
#include <atomic>
#include <unordered_set>
using std::atomic;
using std::unordered_set;

#include "DebuggerTypes.h"

class CPU;
class APU;
class PPU;
class MemoryManager;
class Console;
class Assembler;
class Disassembler;
class LabelManager;
class MemoryDumper;
class MemoryAccessCounter;
class Profiler;
class CodeRunner;
class BaseMapper;
class ScriptHost;
class TraceLogger;
class PerformanceTracker;
class Breakpoint;
class CodeDataLogger;
class ExpressionEvaluator;
class DummyCpu;
class EventManager;
struct ExpressionData;

enum EvalResultType : int32_t;
enum class CdlStripFlag;

class Debugger
{
private:
	static constexpr int BreakpointTypeCount = 8;

	static string _disassemblerOutput;

	shared_ptr<Disassembler> _disassembler;
	shared_ptr<Assembler> _assembler;
	shared_ptr<MemoryDumper> _memoryDumper;
	shared_ptr<CodeDataLogger> _codeDataLogger;
	shared_ptr<MemoryAccessCounter> _memoryAccessCounter;
	shared_ptr<LabelManager> _labelManager;
	shared_ptr<PerformanceTracker> _performanceTracker;
	shared_ptr<EventManager> _eventManager;
	unique_ptr<CodeRunner> _codeRunner;

	shared_ptr<Console> _console;
	shared_ptr<CPU> _cpu;
	shared_ptr<PPU> _ppu;
	shared_ptr<APU> _apu;
	shared_ptr<MemoryManager> _memoryManager;
	shared_ptr<BaseMapper> _mapper;
	
	bool _breakOnFirstCycle;

	bool _hasScript;
	int _nextScriptId;
	vector<shared_ptr<ScriptHost>> _scripts;
	
	vector<Breakpoint> _breakpoints[BreakpointTypeCount];
	vector<ExpressionData> _breakpointRpnList[BreakpointTypeCount];
	bool _hasBreakpoint[BreakpointTypeCount] = {};

	vector<uint8_t> _frozenAddresses;

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
	BreakSource _breakSource;
	
	atomic<bool> _released;

	int64_t _prevInstructionCycle;
	int64_t _curInstructionCycle;
	int64_t _runToCycle;
	bool _needRewind;
	
	vector<stringstream> _rewindCache;
	vector<uint64_t> _rewindPrevInstructionCycleCache;

	uint32_t _inputOverride[4];

public:
	Debugger(shared_ptr<Console> console, shared_ptr<CPU> cpu, shared_ptr<PPU> ppu, shared_ptr<APU> apu, shared_ptr<MemoryManager> memoryManager, shared_ptr<BaseMapper> mapper);
	~Debugger();

	void ReleaseDebugger();

	void SetPpu(shared_ptr<PPU> ppu);
	Console* GetConsole();

	void SetFlags(uint32_t flags);
	bool CheckFlag(DebuggerFlags flag);
	
	shared_ptr<LabelManager> GetLabelManager();

	void GetApuState(ApuState *state);
	void GetState(DebugState *state, bool includeMapperInfo = true);
	void SetState(DebugState state);

	void Resume();

	void Break();

	void PpuStep(uint32_t count = 1);
	void Step(uint32_t count = 1, BreakSource source = BreakSource::CpuStep);
	void StepCycles(uint32_t cycleCount = 1);
	void Run();

	bool LoadCdlFile(string cdlFilepath);
	void SetCdlData(uint8_t* cdlData, uint32_t length);
	void ResetCdl();
	void UpdateCdlCache();
	bool IsMarkedAsCode(uint16_t relativeAddress);
	shared_ptr<CodeDataLogger> GetCodeDataLogger();

	void SetNextStatement(uint16_t addr);

	void GenerateCodeOutput();
	const char* GetCode(uint32_t &length);

	int32_t GetRelativeAddress(uint32_t addr, AddressType type);
	int32_t GetRelativePpuAddress(uint32_t addr, PpuAddressType type);
	int32_t GetAbsoluteAddress(uint32_t addr);	
	int32_t GetAbsoluteChrAddress(uint32_t addr);
	
	void GetAbsoluteAddressAndType(uint32_t relativeAddr, AddressTypeInfo* info);
	void GetPpuAbsoluteAddressAndType(uint32_t relativeAddr, PpuAddressTypeInfo* info);

	shared_ptr<Profiler> GetProfiler();
	shared_ptr<Assembler> GetAssembler();
	shared_ptr<TraceLogger> GetTraceLogger();
	shared_ptr<MemoryDumper> GetMemoryDumper();
	shared_ptr<MemoryAccessCounter> GetMemoryAccessCounter();
	shared_ptr<PerformanceTracker> GetPerformanceTracker();
	shared_ptr<EventManager> GetEventManager();

	void StopCodeRunner();

	void GetNesHeader(uint8_t* header);
	void RevertPrgChrChanges();
	bool HasPrgChrChanges();

	int32_t LoadScript(string name, string content, int32_t scriptId);

	void ProcessEvent(EventType type);

	uint32_t GetScreenPixel(uint8_t x, uint8_t y);
};
