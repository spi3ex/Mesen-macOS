#include "stdafx.h"
#include "VsControlManager.h"
#include "VsSystem.h"
#include "SystemActionManager.h"
#include "VsSystemActionManager.h"

ControllerType VsControlManager::GetControllerType(uint8_t port)
{
	ControllerType type = ControlManager::GetControllerType(port);
	if(type == ControllerType::Zapper) {
		type = ControllerType::VsZapper;
	}
	return type;
}

void VsControlManager::Reset(bool softReset)
{
	ControlManager::Reset(softReset);
	_protectionCounter = 0;

	//Unsure about this, needed for VS Wrecking Crew
	UpdateSlaveMasterBit(_console->IsMaster() ? 0x00 : 0x02);
	
	_vsSystemType = _console->GetRomInfo().VsType;

	if(!softReset && !_console->IsMaster() && _console->GetDualConsole()) {
		UnregisterInputProvider(this);
		RegisterInputProvider(this);
	}
}

VsControlManager::~VsControlManager()
{
	UnregisterInputProvider(this);
}

void VsControlManager::StreamState(bool saving)
{
	ControlManager::StreamState(saving);
	Stream(_prgChrSelectBit, _protectionCounter, _refreshState);
}

void VsControlManager::GetMemoryRanges(MemoryRanges &ranges)
{
	ControlManager::GetMemoryRanges(ranges);
	ranges.AddHandler(MemoryOperation::Read, 0x4020, 0x5FFF);
	ranges.AddHandler(MemoryOperation::Write, 0x4020, 0x5FFF);
}

uint8_t VsControlManager::GetPrgChrSelectBit()
{
	return _prgChrSelectBit;
}

uint8_t VsControlManager::GetOpenBusMask(uint8_t port)
{
	return 0x00;
}

uint8_t VsControlManager::ReadRAM(uint16_t addr)
{
	uint8_t value = 0;

	if(!_console->IsMaster()) {
		//Copy the insert coin 3/4 + service button "2" bits from the main console to this one
		std::shared_ptr<Console> masterConsole = _console->GetDualConsole();
		_systemActionManager->SetBitValue(VsSystemActionManager::VsButtons::InsertCoin1, masterConsole->GetSystemActionManager()->IsPressed(VsSystemActionManager::VsButtons::InsertCoin3));
		_systemActionManager->SetBitValue(VsSystemActionManager::VsButtons::InsertCoin2, masterConsole->GetSystemActionManager()->IsPressed(VsSystemActionManager::VsButtons::InsertCoin4));
		_systemActionManager->SetBitValue(VsSystemActionManager::VsButtons::ServiceButton, masterConsole->GetSystemActionManager()->IsPressed(VsSystemActionManager::VsButtons::ServiceButton2));
	}

	switch(addr) {
		case 0x4016: {
			uint32_t dipSwitches = _console->GetSettings()->GetDipSwitches();
			if(!_console->IsMaster()) {
				dipSwitches >>= 8;
			}

			value = ControlManager::ReadRAM(addr) & 0x65;
			value |= ((dipSwitches & 0x01) ? 0x08 : 0x00);
			value |= ((dipSwitches & 0x02) ? 0x10 : 0x00);
			value |= (_console->IsMaster() ? 0x00 : 0x80);
			break;
		}

		case 0x4017: {
			value = ControlManager::ReadRAM(addr) & 0x01;

			uint32_t dipSwitches = _console->GetSettings()->GetDipSwitches();
			if(!_console->IsMaster()) {
				dipSwitches >>= 8;
			}
			value |= ((dipSwitches & 0x04) ? 0x04 : 0x00);
			value |= ((dipSwitches & 0x08) ? 0x08 : 0x00);
			value |= ((dipSwitches & 0x10) ? 0x10 : 0x00);
			value |= ((dipSwitches & 0x20) ? 0x20 : 0x00);
			value |= ((dipSwitches & 0x40) ? 0x40 : 0x00);
			value |= ((dipSwitches & 0x80) ? 0x80 : 0x00);
			break;
		}

		case 0x5E00:
			_protectionCounter = 0;
			break;

		case 0x5E01:
			if(_vsSystemType == VsSystemType::TkoBoxingProtection) {
				value = _protectionData[0][_protectionCounter++ & 0x1F];
			} else if(_vsSystemType == VsSystemType::RbiBaseballProtection) {
				value = _protectionData[1][_protectionCounter++ & 0x1F];
			}
			break;

		default:
			if(_vsSystemType == VsSystemType::SuperXeviousProtection) {
				return _protectionData[2][_protectionCounter++ & 0x1F];
			}
			break;
	}

	return value;
}

void VsControlManager::WriteRAM(uint16_t addr, uint8_t value)
{
	ControlManager::WriteRAM(addr, value);

	_refreshState = (value & 0x01) == 0x01;

	if(addr == 0x4016) {
		_prgChrSelectBit = (value >> 2) & 0x01;
		
		//Bit 2: DualSystem-only
		uint8_t slaveMasterBit = (value & 0x02);
		if(slaveMasterBit != _slaveMasterBit) {
			UpdateSlaveMasterBit(slaveMasterBit);
		}
	}
}

void VsControlManager::UpdateSlaveMasterBit(uint8_t slaveMasterBit)
{
	std::shared_ptr<Console> dualConsole = _console->GetDualConsole();
	if(dualConsole) {
		VsSystem* mapper = dynamic_cast<VsSystem*>(_console->GetMapper());
		
		if(_console->IsMaster()) {
			mapper->UpdateMemoryAccess(slaveMasterBit);
		}

		if(slaveMasterBit) {
			dualConsole->GetCpu()->ClearIrqSource(IRQSource::External);
		} else {
			//When low, asserts /IRQ on the other CPU
			dualConsole->GetCpu()->SetIrqSource(IRQSource::External);
		}
	}
	_slaveMasterBit = slaveMasterBit;
}

void VsControlManager::UpdateControlDevices()
{
	if(_console->GetDualConsole()) {
		_controlDevices.clear();
		RegisterControlDevice(_systemActionManager);

		//Force 4x standard controllers
		//P3 & P4 will be sent to the slave CPU - see SetInput() below.
		for(int i = 0; i < 4; i++) {
			std::shared_ptr<BaseControlDevice> device = CreateControllerDevice(ControllerType::StandardController, i, _console);
			if(device) {
				RegisterControlDevice(device);
			}
		}
	} else {
		ControlManager::UpdateControlDevices();
	}
}

bool VsControlManager::SetInput(BaseControlDevice* device)
{
	uint8_t port = device->GetPort();
	ControlManager* masterControlManager = _console->GetDualConsole()->GetControlManager();
	if(masterControlManager && port <= 1) {
		std::shared_ptr<BaseControlDevice> controlDevice = masterControlManager->GetControlDevice(port + 2);
		if(controlDevice) {
			ControlDeviceState state = controlDevice->GetRawState();
			device->SetRawState(state);
		}
	}
	return true;
}
