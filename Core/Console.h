#pragma once

#include "stdafx.h"
#include "VirtualFile.h"

class BaseMapper;
class APU;
class CPU;
class PPU;
class MemoryManager;
class ControlManager;
class HdPackBuilder;
class HdAudioDevice;
class SystemActionManager;
class Timer;
class CheatManager;
class SaveStateManager;
class VideoDecoder;
class VideoRenderer;
class DebugHud;
class SoundMixer;
class NotificationManager;
class Debugger;
class EmulationSettings;
class BatteryManager;

struct HdPackData;
struct HashInfo;
struct RomInfo;

enum class MemoryOperationType;
enum class NesModel;
enum class ScaleFilterType;
enum class ConsoleFeatures;
enum class DebugMemoryType;
enum class EventType;
enum class DebugEventType : uint8_t;
enum class RamPowerOnState;

class Console : public std::enable_shared_from_this<Console>
{
private:
	shared_ptr<CPU> _cpu;
	shared_ptr<PPU> _ppu;
	shared_ptr<APU> _apu;
	shared_ptr<BaseMapper> _mapper;
	shared_ptr<ControlManager> _controlManager;
	shared_ptr<MemoryManager> _memoryManager;
	
	//Used by VS-DualSystem
	shared_ptr<Console> _master;
	shared_ptr<Console> _slave;
	
	shared_ptr<BatteryManager> _batteryManager;
	shared_ptr<SystemActionManager> _systemActionManager;

	shared_ptr<VideoDecoder> _videoDecoder;
	shared_ptr<VideoRenderer> _videoRenderer;
	shared_ptr<SaveStateManager> _saveStateManager;
	shared_ptr<CheatManager> _cheatManager;
	shared_ptr<SoundMixer> _soundMixer;
	shared_ptr<NotificationManager> _notificationManager;
	shared_ptr<EmulationSettings> _settings;

	shared_ptr<HdPackBuilder> _hdPackBuilder;
	shared_ptr<HdPackData> _hdData;
	unique_ptr<HdAudioDevice> _hdAudioDevice;

	NesModel _model;

	string _romFilepath;
	string _patchFilename;

	bool _stop = false;
	bool _running = false;

	bool _disableOcNextFrame = false;

	bool _initialized = false;

	void LoadHdPack(VirtualFile &romFile, VirtualFile &patchFile);

	void UpdateNesModel(bool sendNotification);

public:
	Console(shared_ptr<Console> master = nullptr, EmulationSettings* initialSettings = nullptr);
	~Console();

	void Init();
	void Release(bool forShutdown);

	shared_ptr<BatteryManager> GetBatteryManager();
	shared_ptr<SaveStateManager> GetSaveStateManager();
	shared_ptr<VideoDecoder> GetVideoDecoder();
	shared_ptr<VideoRenderer> GetVideoRenderer();
	shared_ptr<DebugHud> GetDebugHud();
	shared_ptr<SoundMixer> GetSoundMixer();
	shared_ptr<NotificationManager> GetNotificationManager();
	EmulationSettings* GetSettings();
	
	bool IsDualSystem();
	shared_ptr<Console> GetDualConsole();
	bool IsMaster();

	void ProcessCpuClock();
	CPU* GetCpu();
	PPU* GetPpu();
	APU* GetApu();
	BaseMapper* GetMapper();
	ControlManager* GetControlManager();
	MemoryManager* GetMemoryManager();
	CheatManager* GetCheatManager();

	bool LoadMatchingRom(string romName, HashInfo hashInfo);
	string FindMatchingRom(string romName, HashInfo hashInfo);

	bool Initialize(string romFile, string patchFile = "");
	bool Initialize(VirtualFile &romFile);
	bool Initialize(VirtualFile &romFile, VirtualFile &patchFile, bool forPowerCycle = false);

	void SaveBatteries();

	void Run();
	void Stop(int stopCode = 0);
		
	void RunSingleFrame();
	void RunSlaveCpu();
	bool UpdateHdPackMode();

	shared_ptr<SystemActionManager> GetSystemActionManager();

	template<typename T>
	shared_ptr<T> GetSystemActionManager()
	{
		return std::dynamic_pointer_cast<T>(_systemActionManager);
	}

	uint32_t GetDipSwitchCount();
	ConsoleFeatures GetAvailableFeatures();
	void InputBarcode(uint64_t barcode, uint32_t digitCount);

	bool IsNsf();
		
	void Reset(bool softReset = true);
	void PowerCycle();
	void ReloadRom(bool forPowerCycle = false);
	void ResetComponents(bool softReset);

	void SaveState(ostream &saveStream);
	void LoadState(istream &loadStream);
	void LoadState(istream &loadStream, uint32_t stateVersion);
	void LoadState(uint8_t *buffer, uint32_t bufferSize);

	VirtualFile GetRomPath();
	VirtualFile GetPatchFile();
	RomInfo GetRomInfo();
	uint32_t GetFrameCount();
	NesModel GetModel();

	void SetNextFrameOverclockStatus(bool disabled);

	void InitializeRam(void* data, uint32_t length);
	static void InitializeRam(RamPowerOnState powerOnState, void* data, uint32_t length);

	shared_ptr<HdPackData> GetHdData();
	bool IsHdPpu();

	void StartRecordingHdPack(string saveFolder, ScaleFilterType filterType, uint32_t scale, uint32_t flags, uint32_t chrRamBankSize);
	void StopRecordingHdPack();
	
	uint8_t* GetRamBuffer(uint16_t address);
		
	void DebugAddTrace(const char *log);
	void DebugProcessPpuCycle();
	void DebugProcessEvent(EventType type);
	void DebugProcessInterrupt(uint16_t cpuAddr, uint16_t destCpuAddr, bool forNmi);
	void DebugSetLastFramePpuScroll(uint16_t addr, uint8_t xScroll, bool updateHorizontalScrollOnly);
	void DebugAddDebugEvent(DebugEventType type);
	bool DebugProcessRamOperation(MemoryOperationType type, uint16_t &addr, uint8_t &value);
	void DebugProcessVramReadOperation(MemoryOperationType type, uint16_t addr, uint8_t &value);
	void DebugProcessVramWriteOperation(uint16_t addr, uint8_t &value);
};
