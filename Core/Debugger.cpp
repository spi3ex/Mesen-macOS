#include "stdafx.h"
#include "../Utilities/FolderUtilities.h"
#include "MessageManager.h"
#include "Debugger.h"
#include "Console.h"
#include "BaseMapper.h"
#include "Disassembler.h"
#include "VideoDecoder.h"
#include "APU.h"
#include "SoundMixer.h"
#include "CodeDataLogger.h"
#include "ExpressionEvaluator.h"
#include "LabelManager.h"
#include "MemoryDumper.h"
#include "MemoryAccessCounter.h"
#include "Assembler.h"
#include "CodeRunner.h"
#include "DisassemblyInfo.h"
#include "PPU.h"
#include "MemoryManager.h"
#include "ScriptHost.h"
#include "StandardController.h"
#include "CodeDataLogger.h"
#include "DummyCpu.h"

string Debugger::_disassemblerOutput = "";

Debugger::Debugger(shared_ptr<Console> console, shared_ptr<CPU> cpu, shared_ptr<PPU> ppu, shared_ptr<APU> apu, shared_ptr<MemoryManager> memoryManager, shared_ptr<BaseMapper> mapper)
{
	_romName = console->GetRomInfo().RomName;
	_console = console;
	_cpu = cpu;
	_apu = apu;
	_memoryManager = memoryManager;
	_mapper = mapper;

	_breakOnFirstCycle = false;

	_labelManager.reset(new LabelManager(_mapper));
	_assembler.reset(new Assembler(_labelManager));
	_disassembler.reset(new Disassembler(memoryManager.get(), mapper.get(), this));
	_codeDataLogger.reset(new CodeDataLogger(this, mapper->GetMemorySize(DebugMemoryType::PrgRom), mapper->GetMemorySize(DebugMemoryType::ChrRom)));

	SetPpu(ppu);

	_memoryAccessCounter.reset(new MemoryAccessCounter(this));

	_opCodeCycle = 0;

	_currentReadAddr = nullptr;
	_currentReadValue = nullptr;
	_nextReadAddr = -1;
	_returnToAddress = 0;

	_flags = 0;

	_prevInstructionCycle = -1;
	_curInstructionCycle = -1;

	_disassemblerOutput = "";
	
	memset(_inputOverride, 0, sizeof(_inputOverride));

	_frozenAddresses.insert(_frozenAddresses.end(), 0x10000, 0);

	if(!LoadCdlFile(FolderUtilities::CombinePath(FolderUtilities::GetDebuggerFolder(), FolderUtilities::GetFilename(_romName, false) + ".cdl"))) {
		_disassembler->Reset();
	}

	_hasScript = false;
	_nextScriptId = 0;

	_released = false;
}

Debugger::~Debugger()
{
	if(!_released) {
		ReleaseDebugger();
	}
}

void Debugger::ReleaseDebugger()
{
	if(!_released) {
		_codeDataLogger->SaveCdlFile(FolderUtilities::CombinePath(FolderUtilities::GetDebuggerFolder(), FolderUtilities::GetFilename(_romName, false) + ".cdl"));

		{
			for(shared_ptr<ScriptHost> script : _scripts) {
				//Send a ScriptEnded event to all active scripts
				script->ProcessEvent(EventType::ScriptEnded);
			}
			_scripts.clear();
			_hasScript = false;
		}

		_released = true;
	}
}

void Debugger::SetPpu(shared_ptr<PPU> ppu)
{
	_ppu = ppu;
	_memoryDumper.reset(new MemoryDumper(_ppu, _memoryManager, _mapper, _codeDataLogger, this, _disassembler));
}

Console* Debugger::GetConsole()
{
	return _console.get();
}

void Debugger::SetFlags(uint32_t flags)
{
	bool needUpdate = ((flags ^ _flags) & (int)DebuggerFlags::DisplayOpCodesInLowerCase) != 0;
	_flags = flags;
	_breakOnFirstCycle = CheckFlag(DebuggerFlags::BreakOnFirstCycle);
	if(needUpdate) {
		_disassembler->BuildOpCodeTables(CheckFlag(DebuggerFlags::DisplayOpCodesInLowerCase));
	}
}

bool Debugger::CheckFlag(DebuggerFlags flag)
{
	return (_flags & (uint32_t)flag) == (uint32_t)flag;
}

bool Debugger::LoadCdlFile(string cdlFilepath)
{
	if(_codeDataLogger->LoadCdlFile(cdlFilepath)) {
		UpdateCdlCache();
		return true;
	}
	return false;
}

void Debugger::SetCdlData(uint8_t* cdlData, uint32_t length)
{
	_codeDataLogger->SetCdlData(cdlData, length);
	UpdateCdlCache();
}

void Debugger::ResetCdl()
{
	_codeDataLogger->Reset();
	UpdateCdlCache();
}

void Debugger::UpdateCdlCache()
{

	_disassembler->Reset();
	for(int i = 0, len = _mapper->GetMemorySize(DebugMemoryType::PrgRom); i < len; i++) {
		if(_codeDataLogger->IsCode(i)) {
			AddressTypeInfo info = { i, AddressType::PrgRom };
			i = _disassembler->BuildCache(info, 0, false, false) - 1;
		}
	}

	_functionEntryPoints.clear();
	for(int i = 0, len = _mapper->GetMemorySize(DebugMemoryType::PrgRom); i < len; i++) {
		if(_codeDataLogger->IsSubEntryPoint(i)) {
			_functionEntryPoints.emplace(i);

			//After resetting the cache, set the entry point flags in the disassembly cache
			AddressTypeInfo info = { i, AddressType::PrgRom };
			_disassembler->BuildCache(info, 0, true, false);
		}
	}
}

bool Debugger::IsMarkedAsCode(uint16_t relativeAddress)
{
	AddressTypeInfo info;
	GetAbsoluteAddressAndType(relativeAddress, &info);
	if(info.Address >= 0 && info.Type == AddressType::PrgRom)
		return _codeDataLogger->IsCode(info.Address);
	return false;
}

shared_ptr<CodeDataLogger> Debugger::GetCodeDataLogger()
{
	return _codeDataLogger;
}

shared_ptr<LabelManager> Debugger::GetLabelManager()
{
	return _labelManager;
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
	if(state.CPU.PC != _cpu->GetPC()) {
		SetNextStatement(state.CPU.PC);
	}
}

void Debugger::GenerateCodeOutput()
{
	State cpuState;
	_cpu->GetState(cpuState);

	_disassemblerOutput.clear();
	_disassemblerOutput.reserve(10000);

	for(uint32_t i = 0; i < 0x10000; i += 0x100) {
		//Merge all sequential ranges into 1 chunk
		AddressTypeInfo startInfo, currentInfo, endInfo;
		GetAbsoluteAddressAndType(i, &startInfo);
		currentInfo = startInfo;
		GetAbsoluteAddressAndType(i+0x100, &endInfo);

		uint32_t startMemoryAddr = i;
		int32_t startAddr, endAddr;

		if(startInfo.Address >= 0) {
			startAddr = startInfo.Address;
			endAddr = startAddr + 0xFF;
			while(currentInfo.Type == endInfo.Type && currentInfo.Address + 0x100 == endInfo.Address && i < 0x10000) {
				endAddr += 0x100;
				currentInfo = endInfo;
				i+=0x100;
				GetAbsoluteAddressAndType(i + 0x100, &endInfo);
			}
			_disassemblerOutput += _disassembler->GetCode(startInfo, endAddr, startMemoryAddr, cpuState, _memoryManager, _labelManager);
		}
	}
}

const char* Debugger::GetCode(uint32_t &length)
{
	string previousCode = _disassemblerOutput;
	GenerateCodeOutput();
	bool forceRefresh = length == (uint32_t)-1;
	length = (uint32_t)_disassemblerOutput.size();
	if(!forceRefresh && previousCode.compare(_disassemblerOutput) == 0)
		//Return null pointer if the code is identical to last call
		//This avoids the UTF8->UTF16 conversion that the UI 
                //needs to do
		//before comparing the strings
		return nullptr;
	return _disassemblerOutput.c_str();
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

void Debugger::SetNextStatement(uint16_t addr)
{
	if(_currentReadAddr) {
		_cpu->SetDebugPC(addr);
		*_currentReadAddr = addr;
		*_currentReadValue = _memoryManager->DebugRead(addr, false);
	} else {
		//Can't change the address right away (CPU is in the middle of an instruction)
		//Address will change after current instruction is done executing
		_nextReadAddr = addr;
	}
}

shared_ptr<Assembler> Debugger::GetAssembler()
{
	return _assembler;
}

shared_ptr<MemoryDumper> Debugger::GetMemoryDumper()
{
	return _memoryDumper;
}

shared_ptr<MemoryAccessCounter> Debugger::GetMemoryAccessCounter()
{
	return _memoryAccessCounter;
}

void Debugger::GetAbsoluteAddressAndType(uint32_t relativeAddr, AddressTypeInfo* info)
{
	return _mapper->GetAbsoluteAddressAndType(relativeAddr, info);
}

void Debugger::GetPpuAbsoluteAddressAndType(uint32_t relativeAddr, PpuAddressTypeInfo* info)
{
	return _mapper->GetPpuAbsoluteAddressAndType(relativeAddr, info);
}

void Debugger::StopCodeRunner()
{
	_memoryManager->UnregisterIODevice(_codeRunner.get());
	_memoryManager->RegisterIODevice(_ppu.get());
	
	//Break debugger when code has finished executing
	SetNextStatement(_returnToAddress);
}

void Debugger::GetNesHeader(uint8_t* header)
{
	NESHeader nesHeader = _mapper->GetRomInfo().NesHeader;
	memcpy(header, &nesHeader, sizeof(NESHeader));
}

void Debugger::RevertPrgChrChanges()
{
	_mapper->RevertPrgChrChanges();
	_disassembler->Reset();
	UpdateCdlCache();
}

bool Debugger::HasPrgChrChanges()
{
	return _mapper->HasPrgChrChanges();
}

int Debugger::LoadScript(string name, string content, int32_t scriptId)
{
	if(scriptId < 0) {
		shared_ptr<ScriptHost> script(new ScriptHost(_nextScriptId++));
		script->LoadScript(name, content, this);
		_scripts.push_back(script);
		_hasScript = true;
		return script->GetScriptId();
	} else {
		auto result = std::find_if(_scripts.begin(), _scripts.end(), [=](shared_ptr<ScriptHost> &script) {
			return script->GetScriptId() == scriptId;
		});
		if(result != _scripts.end()) {
			//Send a ScriptEnded event before reloading the code
			(*result)->ProcessEvent(EventType::ScriptEnded);

			(*result)->LoadScript(name, content, this);
			return scriptId;
		}
	}

	return -1;
}

void Debugger::ProcessEvent(EventType type)
{
	if(_hasScript) {
		for(shared_ptr<ScriptHost> &script : _scripts) {
			script->ProcessEvent(type);
		}
	}
	switch(type) {
		case EventType::InputPolled:
			for(int i = 0; i < 4; i++) {
				if(_inputOverride[i] != 0) {
					shared_ptr<StandardController> controller = std::dynamic_pointer_cast<StandardController>(_console->GetControlManager()->GetControlDevice(i));
					if(controller) {
						controller->SetBitValue(StandardController::Buttons::A, (_inputOverride[i] & 0x01) != 0);
						controller->SetBitValue(StandardController::Buttons::B, (_inputOverride[i] & 0x02) != 0);
						controller->SetBitValue(StandardController::Buttons::Select, (_inputOverride[i] & 0x04) != 0);
						controller->SetBitValue(StandardController::Buttons::Start, (_inputOverride[i] & 0x08) != 0);
						controller->SetBitValue(StandardController::Buttons::Up, (_inputOverride[i] & 0x10) != 0);
						controller->SetBitValue(StandardController::Buttons::Down, (_inputOverride[i] & 0x20) != 0);
						controller->SetBitValue(StandardController::Buttons::Left, (_inputOverride[i] & 0x40) != 0);
						controller->SetBitValue(StandardController::Buttons::Right, (_inputOverride[i] & 0x80) != 0);
					}
				}
			}
			break;

		case EventType::EndFrame:
			_memoryDumper->GatherChrPaletteInfo();
			break;

		case EventType::StartFrame:
			//Clear the current frame/event log
			if(CheckFlag(DebuggerFlags::PpuPartialDraw)) {
				_ppu->DebugUpdateFrameBuffer(CheckFlag(DebuggerFlags::PpuShowPreviousFrame));
			}

			break;

		case EventType::Nmi: break;
		case EventType::Irq: break;
		case EventType::SpriteZeroHit: break;
		case EventType::Reset: break;

		case EventType::BusConflict: 
			break;

		default: break;
	}
}

uint32_t Debugger::GetScreenPixel(uint8_t x, uint8_t y)
{
	return _ppu->GetPixel(x, y);
}
