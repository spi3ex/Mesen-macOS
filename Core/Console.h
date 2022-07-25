#pragma once

#include "stdafx.h"
#include <memory>
#include "VirtualFile.h"
#include "../Libretro/libretro.h"

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
	std::shared_ptr<CPU> _cpu;
	std::shared_ptr<PPU> _ppu;
	std::shared_ptr<APU> _apu;
	std::shared_ptr<BaseMapper> _mapper;
	std::shared_ptr<ControlManager> _controlManager;
	std::shared_ptr<MemoryManager> _memoryManager;
	
	//Used by VS-DualSystem
	std::shared_ptr<Console> _master;
	std::shared_ptr<Console> _slave;
	
	std::shared_ptr<BatteryManager> _batteryManager;
	std::shared_ptr<SystemActionManager> _systemActionManager;

	std::shared_ptr<VideoDecoder> _videoDecoder;
	std::shared_ptr<VideoRenderer> _videoRenderer;
	std::shared_ptr<SaveStateManager> _saveStateManager;
	std::shared_ptr<CheatManager> _cheatManager;
	std::shared_ptr<SoundMixer> _soundMixer;
	std::shared_ptr<EmulationSettings> _settings;

	std::shared_ptr<HdPackBuilder> _hdPackBuilder;
	std::shared_ptr<HdPackData> _hdData;
	std::unique_ptr<HdAudioDevice> _hdAudioDevice;

	NesModel _model;

	string _romFilepath;
	string _patchFilename;

	bool _disableOcNextFrame = false;

	bool _initialized = false;

	void LoadHdPack(VirtualFile &romFile, VirtualFile &patchFile);

	void UpdateNesModel(bool sendNotification);

public:
	Console(std::shared_ptr<Console> master = nullptr, EmulationSettings* initialSettings = nullptr);
	~Console();

	void Init(retro_environment_t retroEnv);
	void Release(bool forShutdown);

	std::shared_ptr<BatteryManager> GetBatteryManager();
	std::shared_ptr<SaveStateManager> GetSaveStateManager();
	std::shared_ptr<VideoDecoder> GetVideoDecoder();
	std::shared_ptr<VideoRenderer> GetVideoRenderer();
	std::shared_ptr<SoundMixer> GetSoundMixer();
	EmulationSettings* GetSettings();
	
	bool IsDualSystem();
	std::shared_ptr<Console> GetDualConsole();
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

	void RunSingleFrame();
	void RunSlaveCpu();
	bool UpdateHdPackMode();

	std::shared_ptr<SystemActionManager> GetSystemActionManager();

	template<typename T>
	std::shared_ptr<T> GetSystemActionManager()
	{
		return std::dynamic_pointer_cast<T>(_systemActionManager);
	}

	uint32_t GetDipSwitchCount();
	ConsoleFeatures GetAvailableFeatures();
	void InputBarcode(uint64_t barcode, uint32_t digitCount);

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

	std::shared_ptr<HdPackData> GetHdData();
	bool IsHdPpu();

	void StartRecordingHdPack(string saveFolder, ScaleFilterType filterType, uint32_t scale, uint32_t flags, uint32_t chrRamBankSize);
	void StopRecordingHdPack();
	
	uint8_t* GetRamBuffer(uint16_t address);
};
