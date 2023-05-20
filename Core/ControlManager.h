#pragma once

#include "stdafx.h"
#include <memory>
#include "IMemoryHandler.h"
#include "Snapshotable.h"

class BaseControlDevice;
class Zapper;
class SystemActionManager;
class IInputRecorder;
class IInputProvider;
class Console;
struct ControlDeviceState;
enum class ControllerType;
enum class ExpansionPortDevice;

class ControlManager : public Snapshotable, public IMemoryHandler
{
private:
	vector<IInputRecorder*> _inputRecorders;
	vector<IInputProvider*> _inputProviders;
	
	//Static so that power cycle does not reset its value
	uint32_t _pollCounter;

	std::shared_ptr<BaseControlDevice> _mapperControlDevice;

	uint32_t _lagCounter = 0;
	bool _isLagging = false;

protected:
	std::shared_ptr<Console> _console;
	vector<std::shared_ptr<BaseControlDevice>> _controlDevices;
	std::shared_ptr<BaseControlDevice> _systemActionManager;

	void RegisterControlDevice(std::shared_ptr<BaseControlDevice> controlDevice);

	virtual void StreamState(bool saving) override;
	virtual ControllerType GetControllerType(uint8_t port);
	virtual void RemapControllerButtons();

public:
	ControlManager(std::shared_ptr<Console> console, std::shared_ptr<BaseControlDevice> systemActionManager, std::shared_ptr<BaseControlDevice> mapperControlDevice);
	virtual ~ControlManager();

	virtual uint8_t GetOpenBusMask(uint8_t port);

	virtual void UpdateControlDevices();
	void UpdateInputState();

	void ResetLagCounter();

	uint32_t GetPollCounter();
	void SetPollCounter(uint32_t value);

	virtual void Reset(bool softReset);

	void RegisterInputProvider(IInputProvider* provider);
	void UnregisterInputProvider(IInputProvider* provider);

	void RegisterInputRecorder(IInputRecorder* recorder);
	void UnregisterInputRecorder(IInputRecorder* recorder);

	std::shared_ptr<BaseControlDevice> GetControlDevice(uint8_t port);
	vector<std::shared_ptr<BaseControlDevice>> GetControlDevices();
	bool HasKeyboard();
	
	static std::shared_ptr<BaseControlDevice> CreateControllerDevice(ControllerType type, uint8_t port, std::shared_ptr<Console> console);
	static std::shared_ptr<BaseControlDevice> CreateExpansionDevice(ExpansionPortDevice type, std::shared_ptr<Console> console);

	virtual void GetMemoryRanges(MemoryRanges &ranges) override
	{
		ranges.AddHandler(MemoryOperation::Read, 0x4016, 0x4017);
		ranges.AddHandler(MemoryOperation::Write, 0x4016);
	}

	virtual uint8_t ReadRAM(uint16_t addr) override;
	virtual void WriteRAM(uint16_t addr, uint8_t value) override;
};
