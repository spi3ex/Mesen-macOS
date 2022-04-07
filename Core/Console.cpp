#include "stdafx.h"
#include <random>
#include "Console.h"
#include "CPU.h"
#include "PPU.h"
#include "APU.h"
#include "MemoryManager.h"
#include "BaseMapper.h"
#include "ControlManager.h"
#include "VsControlManager.h"
#include "MapperFactory.h"
#include "EmulationSettings.h"
#include "../Utilities/FolderUtilities.h"
#include "VirtualFile.h"
#include "HdBuilderPpu.h"
#include "HdPpu.h"
#include "SoundMixer.h"
#include "NsfMapper.h"
#include "SaveStateManager.h"
#include "HdPackBuilder.h"
#include "HdAudioDevice.h"
#include "FDS.h"
#include "SystemActionManager.h"
#include "FdsSystemActionManager.h"
#include "VsSystemActionManager.h"
#include "FamilyBasicDataRecorder.h"
#include "IBarcodeReader.h"
#include "IBattery.h"
#include "KeyManager.h"
#include "BatteryManager.h"
#include "RomLoader.h"
#include "CheatManager.h"
#include "VideoDecoder.h"
#include "VideoRenderer.h"

Console::Console(shared_ptr<Console> master, EmulationSettings* initialSettings)
{
	_master = master;
	
	if(_master) {
		//Slave console should use the same settings as the master
		_settings = _master->_settings;
	} else {
		if(initialSettings) {
			_settings.reset(new EmulationSettings(*initialSettings));
		} else {
			_settings.reset(new EmulationSettings());
		}
		KeyManager::SetSettings(_settings.get());
	}

	_model = NesModel::NTSC;
}

Console::~Console()
{
}

void Console::Init(retro_environment_t retroEnv)
{
	_batteryManager.reset(new BatteryManager());
	
	_videoRenderer.reset(new VideoRenderer(shared_from_this(), retroEnv));
	_videoDecoder.reset(new VideoDecoder(shared_from_this()));

	_saveStateManager.reset(new SaveStateManager(shared_from_this()));
	_cheatManager.reset(new CheatManager(shared_from_this()));

	_soundMixer.reset(new SoundMixer(shared_from_this()));
	_soundMixer->SetNesModel(_model);
}

void Console::Release(bool forShutdown)
{
	if(_slave) {
		_slave->Release(true);
		_slave.reset();
	}

	if(forShutdown) {
		_videoDecoder.reset();
		_videoRenderer.reset();

		_saveStateManager.reset();
		_cheatManager.reset();

		_soundMixer.reset();
	}

	_hdPackBuilder.reset();
	_hdData.reset();
	_hdAudioDevice.reset();

	_systemActionManager.reset();
	
	_master.reset();
	_cpu.reset();
	_ppu.reset();
	_apu.reset();
	_mapper.reset();
	_memoryManager.reset();
	_controlManager.reset();
}

shared_ptr<BatteryManager> Console::GetBatteryManager()
{
	return _batteryManager;
}

shared_ptr<SaveStateManager> Console::GetSaveStateManager()
{
	return _saveStateManager;
}

shared_ptr<VideoDecoder> Console::GetVideoDecoder()
{
	return _videoDecoder;
}

shared_ptr<VideoRenderer> Console::GetVideoRenderer()
{
	return _videoRenderer;
}

void Console::SaveBatteries()
{
	shared_ptr<BaseMapper> mapper = _mapper;
	shared_ptr<ControlManager> controlManager = _controlManager;
	
	if(mapper) {
		mapper->SaveBattery();
	}

	if(controlManager) {
		shared_ptr<IBattery> device = std::dynamic_pointer_cast<IBattery>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort));
		if(device) {
			device->SaveBattery();
		}
	}
}

bool Console::LoadMatchingRom(string romName, HashInfo hashInfo)
{
	if(_initialized) {
		string currentRomFilepath = GetRomPath().GetFilePath();
		if(!currentRomFilepath.empty()) {
			HashInfo gameHashInfo = GetRomInfo().Hash;
			if(gameHashInfo.Crc32 == hashInfo.Crc32 || gameHashInfo.Sha1.compare(hashInfo.Sha1) == 0 || gameHashInfo.PrgChrMd5.compare(hashInfo.PrgChrMd5) == 0) {
				//Current game matches, power cycle game and return
				PowerCycle();
				return true;
			}
		}
	}

	string match = FindMatchingRom(romName, hashInfo);
	if(!match.empty()) {
		return Initialize(match);
	}
	return false;
}

string Console::FindMatchingRom(string romName, HashInfo hashInfo)
{
	if(_initialized) {
		VirtualFile currentRom = GetRomPath();
		if(currentRom.IsValid() && !GetPatchFile().IsValid()) {
			HashInfo gameHashInfo = GetRomInfo().Hash;
			if(gameHashInfo.Crc32 == hashInfo.Crc32 || gameHashInfo.Sha1.compare(hashInfo.Sha1) == 0 || gameHashInfo.PrgChrMd5.compare(hashInfo.PrgChrMd5) == 0) {
				//Current game matches
				return currentRom;
			}
		}
	}

	string lcRomname = romName;
	std::transform(lcRomname.begin(), lcRomname.end(), lcRomname.begin(), ::tolower);
	std::unordered_set<string> validExtensions = { { ".nes", ".fds", "*.unif", "*.unif", "*.nsf", "*.nsfe", "*.7z", "*.zip" } };
	vector<string> romFiles;
	for(string folder : FolderUtilities::GetKnownGameFolders()) {
		vector<string> files = FolderUtilities::GetFilesInFolder(folder, validExtensions, true);
		romFiles.insert(romFiles.end(), files.begin(), files.end());
	}

	if(!romName.empty()) {
		//Perform quick search based on file name
		string match = RomLoader::FindMatchingRom(romFiles, romName, hashInfo, true);
		if(!match.empty()) {
			return match;
		}
	}

	//Perform slow CRC32 search for ROM
	string match = RomLoader::FindMatchingRom(romFiles, romName, hashInfo, false);
	if(!match.empty()) {
		return match;
	}

	return "";
}

bool Console::Initialize(string romFile, string patchFile)
{
	VirtualFile rom = romFile;
	VirtualFile patch = patchFile;
	return Initialize(rom, patch);
}

bool Console::Initialize(VirtualFile &romFile)
{
	VirtualFile patchFile;
	return Initialize(romFile, patchFile);
}

bool Console::Initialize(VirtualFile &romFile, VirtualFile &patchFile, bool forPowerCycle)
{
	if(romFile.IsValid()) {
		if(!_romFilepath.empty() && _mapper) {
			//Ensure we save any battery file before loading a new game
			SaveBatteries();
		}

		shared_ptr<HdPackData> originalHdPackData = _hdData;
		LoadHdPack(romFile, patchFile);
		if(patchFile.IsValid())
			romFile.ApplyPatch(patchFile);

		_batteryManager->Initialize(FolderUtilities::GetFilename(romFile.GetFileName(), false));

		RomData romData;
		shared_ptr<BaseMapper> mapper = MapperFactory::InitializeFromFile(shared_from_this(), romFile, romData);
		if(mapper) {
			bool isDifferentGame = _romFilepath != (string)romFile || _patchFilename != (string)patchFile;

			if(isDifferentGame) {
				_romFilepath = romFile;
				_patchFilename = patchFile;
				
				//Changed game, stop all recordings
				_soundMixer->StopRecording();
				StopRecordingHdPack();
			}

			shared_ptr<BaseMapper> previousMapper = _mapper;
			_mapper = mapper;
			_memoryManager.reset(new MemoryManager(shared_from_this()));
			_cpu.reset(new CPU(shared_from_this()));
			_apu.reset(new APU(shared_from_this()));

			_mapper->SetConsole(shared_from_this());
			_mapper->Initialize(romData);
			if(!isDifferentGame && forPowerCycle) {
				_mapper->CopyPrgChrRom(previousMapper);
			}

			if(_slave) {
				_slave->Release(false);
				_slave.reset();
			}

			RomInfo romInfo = _mapper->GetRomInfo();
			if(!_master && romInfo.VsType == VsSystemType::VsDualSystem) {
				extern retro_environment_t env_cb;
				_slave.reset(new Console(shared_from_this()));
				_slave->Init(env_cb);
				_slave->Initialize(romFile, patchFile);
			}

			switch(romInfo.System) {
				case GameSystem::FDS:
					_settings->SetPpuModel(PpuModel::Ppu2C02);
					_systemActionManager.reset(new FdsSystemActionManager(shared_from_this(), _mapper));
					break;
				
				case GameSystem::VsSystem:
					_settings->SetPpuModel(romInfo.VsPpuModel);
					_systemActionManager.reset(new VsSystemActionManager(shared_from_this()));
					break;
				
				default: 
					_settings->SetPpuModel(PpuModel::Ppu2C02);
					_systemActionManager.reset(new SystemActionManager(shared_from_this())); break;
			}

			//Temporarely disable battery saves to prevent battery files from being created for the wrong game (for Battle Box & Turbo File)
			_batteryManager->SetSaveEnabled(false);

			uint32_t pollCounter = 0;
			if(_controlManager && !isDifferentGame) {
				//When power cycling, poll counter must be preserved to allow movies to playback properly
				pollCounter = _controlManager->GetPollCounter();
			}

			if(romInfo.System == GameSystem::VsSystem) {
				_controlManager.reset(new VsControlManager(shared_from_this(), _systemActionManager, _mapper->GetMapperControlDevice()));
			} else {
				_controlManager.reset(new ControlManager(shared_from_this(), _systemActionManager, _mapper->GetMapperControlDevice()));
			}
			_controlManager->SetPollCounter(pollCounter);
			_controlManager->UpdateControlDevices();
			
			//Re-enable battery saves
			_batteryManager->SetSaveEnabled(true);
			
			if(_hdData && (!_hdData->Tiles.empty() || !_hdData->Backgrounds.empty())) {
				_ppu.reset(new HdPpu(shared_from_this(), _hdData.get()));
			} else {
				_ppu.reset(new PPU(shared_from_this()));
			}

			_memoryManager->SetMapper(_mapper);
			_memoryManager->RegisterIODevice(_ppu.get());
			_memoryManager->RegisterIODevice(_apu.get());
			_memoryManager->RegisterIODevice(_controlManager.get());
			_memoryManager->RegisterIODevice(_mapper.get());

			if(_hdData && (!_hdData->BgmFilesById.empty() || !_hdData->SfxFilesById.empty())) {
				_hdAudioDevice.reset(new HdAudioDevice(shared_from_this(), _hdData.get()));
				_memoryManager->RegisterIODevice(_hdAudioDevice.get());
			} else {
				_hdAudioDevice.reset();
			}

			_model = NesModel::Auto;
			UpdateNesModel(false);

			_initialized = true;

			ResetComponents(false);

			//Poll controller input after creating rewind manager, to make sure it catches the first frame's input
			_controlManager->UpdateInputState();

			FolderUtilities::AddKnownGameFolder(romFile.GetFolderPath());

			if(IsMaster()) {
				_settings->ClearFlags(EmulationFlags::ForceMaxSpeed);
			}
			return true;
		} else {
			_hdData = originalHdPackData;

			//Reset battery source to current game if new game failed to load
			_batteryManager->Initialize(FolderUtilities::GetFilename(GetRomInfo().RomName, false));
		}
	}

	return false;
}

void Console::ProcessCpuClock()
{
	_mapper->ProcessCpuClock();
	_apu->ProcessCpuClock();
}

CPU* Console::GetCpu()
{
	return _cpu.get();
}

PPU* Console::GetPpu()
{
	return _ppu.get();
}

APU* Console::GetApu()
{
	return _apu.get();
}

shared_ptr<SoundMixer> Console::GetSoundMixer()
{
	return _soundMixer;
}

EmulationSettings* Console::GetSettings()
{
	return _settings.get();
}

bool Console::IsDualSystem()
{
	return _slave != nullptr || _master != nullptr;
}

shared_ptr<Console> Console::GetDualConsole()
{
	//When called from the master, returns the slave.
	//When called from the slave, returns the master.
	//Returns a null pointer when not running a dualsystem game
	return _slave ? _slave : _master;
}

bool Console::IsMaster()
{
	return !_master;
}

BaseMapper* Console::GetMapper()
{
	return _mapper.get();
}

ControlManager* Console::GetControlManager()
{
	return _controlManager.get();
}

MemoryManager* Console::GetMemoryManager()
{
	return _memoryManager.get();
}

CheatManager* Console::GetCheatManager()
{
	return _cheatManager.get();
}

VirtualFile Console::GetRomPath()
{
	return static_cast<VirtualFile>(_romFilepath);
}

VirtualFile Console::GetPatchFile()
{
	return (VirtualFile)_patchFilename;
}

RomInfo Console::GetRomInfo()
{
	return _mapper ? _mapper->GetRomInfo() : (RomInfo {});
}

uint32_t Console::GetFrameCount()
{
	return _ppu ? _ppu->GetFrameCount() : 0;
}

NesModel Console::GetModel()
{
	return _model;
}

shared_ptr<SystemActionManager> Console::GetSystemActionManager()
{
	return _systemActionManager;
}

void Console::PowerCycle()
{
	ReloadRom(true);
}

void Console::ReloadRom(bool forPowerCycle)
{
	if(_initialized && !_romFilepath.empty()) {
		VirtualFile romFile = _romFilepath;
		VirtualFile patchFile = _patchFilename;
		Initialize(romFile, patchFile, forPowerCycle);
	}
}

void Console::Reset(bool softReset)
{
	if(_initialized) {
		if (softReset)
			_systemActionManager->Reset();
		else
			_systemActionManager->PowerCycle();
	}
}

void Console::ResetComponents(bool softReset)
{
	if(_slave) {
		//Always reset/power cycle the slave alongside the master CPU
		_slave->ResetComponents(softReset);
	}

	_memoryManager->Reset(softReset);
	if(!_settings->CheckFlag(EmulationFlags::DisablePpuReset) || !softReset || IsNsf()) {
		_ppu->Reset();
	}
	_apu->Reset(softReset);
	_cpu->Reset(softReset, _model);
	_controlManager->Reset(softReset);
}

void Console::RunSingleFrame()
{
	//Used by Libretro
	uint32_t lastFrameNumber = _ppu->GetFrameCount();
	UpdateNesModel(true);

	while(_ppu->GetFrameCount() == lastFrameNumber) {
		_cpu->Exec();
		if(_slave)
			RunSlaveCpu();
	}

	_settings->DisableOverclocking(_disableOcNextFrame || IsNsf());
	_disableOcNextFrame = false;

	_systemActionManager->ProcessSystemActions();
	_apu->EndFrame();
}

void Console::RunSlaveCpu()
{
	int64_t cycleGap;
	while(true) {
		//Run the slave until it catches up to the master CPU (and take into account the CPU count overflow that occurs every ~20mins)
		cycleGap = (int64_t)(_cpu->GetCycleCount() - _slave->_cpu->GetCycleCount());
		if(cycleGap > 5 || _ppu->GetFrameCount() > _slave->_ppu->GetFrameCount()) {
			_slave->_cpu->Exec();
		} else {
			break;
		}
	}
}

void Console::UpdateNesModel(bool sendNotification)
{
	if(_settings->NeedControllerUpdate())
		_controlManager->UpdateControlDevices();

	NesModel model = _settings->GetNesModel();
	if(model == NesModel::Auto) {
		switch(_mapper->GetRomInfo().System) {
			case GameSystem::NesPal: model = NesModel::PAL; break;
			case GameSystem::Dendy: model = NesModel::Dendy; break;
			default: model = NesModel::NTSC; break;
		}
	}
	if(_model != model)
		_model = model;

	_cpu->SetMasterClockDivider(model);
	_mapper->SetNesModel(model);
	_ppu->SetNesModel(model);
	_apu->SetNesModel(model);
}

void Console::SaveState(ostream &saveStream)
{
	if(_initialized) {
		//Send any unprocessed sound to the SoundMixer - needed for rewind
		_apu->EndFrame();

		_cpu->SaveSnapshot(&saveStream);
		_ppu->SaveSnapshot(&saveStream);
		_memoryManager->SaveSnapshot(&saveStream);
		_apu->SaveSnapshot(&saveStream);
		_controlManager->SaveSnapshot(&saveStream);
		_mapper->SaveSnapshot(&saveStream);
		if(_hdAudioDevice) {
			_hdAudioDevice->SaveSnapshot(&saveStream);
		} else {
			Snapshotable::WriteEmptyBlock(&saveStream);
		}

		if(_slave) {
			//For VS Dualsystem, append the 2nd console's savestate
			_slave->SaveState(saveStream);
		}
	}
}

void Console::LoadState(istream &loadStream)
{
	LoadState(loadStream, SaveStateManager::FileFormatVersion);
}

void Console::LoadState(istream &loadStream, uint32_t stateVersion)
{
	if(_initialized) {
		//Send any unprocessed sound to the SoundMixer - needed for rewind
		_apu->EndFrame();

		_cpu->LoadSnapshot(&loadStream, stateVersion);
		_ppu->LoadSnapshot(&loadStream, stateVersion);
		_memoryManager->LoadSnapshot(&loadStream, stateVersion);
		_apu->LoadSnapshot(&loadStream, stateVersion);
		_controlManager->LoadSnapshot(&loadStream, stateVersion);
		_mapper->LoadSnapshot(&loadStream, stateVersion);
		if(_hdAudioDevice) {
			_hdAudioDevice->LoadSnapshot(&loadStream, stateVersion);
		} else {
			Snapshotable::SkipBlock(&loadStream);
		}

		if(_slave) {
			//For VS Dualsystem, the slave console's savestate is appended to the end of the file
			_slave->LoadState(loadStream, stateVersion);
		}
		
		UpdateNesModel(false);
	}
}

void Console::LoadState(uint8_t *buffer, uint32_t bufferSize)
{
	stringstream stream;
	stream.write((char*)buffer, bufferSize);
	stream.seekg(0, ios::beg);
	LoadState(stream);
}

void Console::SetNextFrameOverclockStatus(bool disabled)
{
	_disableOcNextFrame = disabled;
}

void Console::InitializeRam(void* data, uint32_t length)
{
	InitializeRam(_settings->GetRamPowerOnState(), data, length);
}

void Console::InitializeRam(RamPowerOnState powerOnState, void* data, uint32_t length)
{
	switch(powerOnState) {
		default:
		case RamPowerOnState::AllZeros: memset(data, 0, length); break;
		case RamPowerOnState::AllOnes: memset(data, 0xFF, length); break;
		case RamPowerOnState::Random:
			std::random_device rd;
			std::mt19937 mt(rd());
			std::uniform_int_distribution<> dist(0, 255);
			for(uint32_t i = 0; i < length; i++) {
				((uint8_t*)data)[i] = dist(mt);
			}
			break;
	}
}

shared_ptr<HdPackData> Console::GetHdData()
{
	return _hdData;
}

bool Console::IsHdPpu()
{
	return _hdData && std::dynamic_pointer_cast<HdPpu>(_ppu) != nullptr;
}

void Console::LoadHdPack(VirtualFile &romFile, VirtualFile &patchFile)
{
	_hdData.reset();
	if(_settings->CheckFlag(EmulationFlags::UseHdPacks)) {
		_hdData.reset(new HdPackData());
		if(!HdPackLoader::LoadHdNesPack(romFile, *_hdData.get())) {
			_hdData.reset();
		} else {
			auto result = _hdData->PatchesByHash.find(romFile.GetSha1Hash());
			if(result != _hdData->PatchesByHash.end()) {
				patchFile = result->second;
			}
		}
	}
}

void Console::StartRecordingHdPack(string saveFolder, ScaleFilterType filterType, uint32_t scale, uint32_t flags, uint32_t chrRamBankSize)
{
	std::stringstream saveState;
	SaveState(saveState);
	
	_hdPackBuilder.reset();
	_hdPackBuilder.reset(new HdPackBuilder(shared_from_this(), saveFolder, filterType, scale, flags, chrRamBankSize, !_mapper->HasChrRom()));

	_memoryManager->UnregisterIODevice(_ppu.get());
	_ppu.reset();
	_ppu.reset(new HdBuilderPpu(shared_from_this(), _hdPackBuilder.get(), chrRamBankSize, _hdData));
	_memoryManager->RegisterIODevice(_ppu.get());

	LoadState(saveState);
}

void Console::StopRecordingHdPack()
{
	if(_hdPackBuilder) {
		std::stringstream saveState;
		SaveState(saveState);

		_memoryManager->UnregisterIODevice(_ppu.get());
		_ppu.reset();
		if(_hdData && (!_hdData->Tiles.empty() || !_hdData->Backgrounds.empty())) {
			_ppu.reset(new HdPpu(shared_from_this(), _hdData.get()));
		} else {
			_ppu.reset(new PPU(shared_from_this()));
		}
		_memoryManager->RegisterIODevice(_ppu.get());
		_hdPackBuilder.reset();

		LoadState(saveState);
	}
}

bool Console::UpdateHdPackMode()
{
	//Switch back and forth between HD PPU and regular PPU as needed

	VirtualFile romFile = _romFilepath;
	VirtualFile patchFile = _patchFilename;
	LoadHdPack(romFile, patchFile);

	bool isHdPackLoaded = std::dynamic_pointer_cast<HdPpu>(_ppu) != nullptr;
	bool hdPackFound = _hdData && (!_hdData->Tiles.empty() || !_hdData->Backgrounds.empty());

	bool modeChanged = false;
	if(isHdPackLoaded != hdPackFound) {
		std::stringstream saveState;
		SaveState(saveState);

		_memoryManager->UnregisterIODevice(_ppu.get());
		_ppu.reset();
		if(_hdData && (!_hdData->Tiles.empty() || !_hdData->Backgrounds.empty())) {
			_ppu.reset(new HdPpu(shared_from_this(), _hdData.get()));
		} else {
			_ppu.reset(new PPU(shared_from_this()));
		}
		_memoryManager->RegisterIODevice(_ppu.get());

		LoadState(saveState);
		modeChanged = true;
	}

	return modeChanged;
}

uint32_t Console::GetDipSwitchCount()
{
	shared_ptr<ControlManager> controlManager = _controlManager;
	shared_ptr<BaseMapper> mapper = _mapper;
	
	if(std::dynamic_pointer_cast<VsControlManager>(controlManager)) {
		return IsDualSystem() ? 16 : 8;
	} else if(mapper) {
		return mapper->GetMapperDipSwitchCount();
	}

	return 0;
}

ConsoleFeatures Console::GetAvailableFeatures()
{
	ConsoleFeatures features = ConsoleFeatures::None;
	shared_ptr<BaseMapper> mapper = _mapper;
	shared_ptr<ControlManager> controlManager = _controlManager;
	if(mapper && controlManager) {
		features = (ConsoleFeatures)((int)features | (int)mapper->GetAvailableFeatures());

		if(dynamic_cast<VsControlManager*>(controlManager.get())) {
			features = (ConsoleFeatures)((int)features | (int)ConsoleFeatures::VsSystem);
		}

		if(std::dynamic_pointer_cast<IBarcodeReader>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort))) {
			features = (ConsoleFeatures)((int)features | (int)ConsoleFeatures::BarcodeReader);
		}

		if(std::dynamic_pointer_cast<FamilyBasicDataRecorder>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort2))) {
			features = (ConsoleFeatures)((int)features | (int)ConsoleFeatures::TapeRecorder);
		}
	}
	return features;
}

void Console::InputBarcode(uint64_t barcode, uint32_t digitCount)
{
	shared_ptr<BaseMapper> mapper = _mapper;
	shared_ptr<ControlManager> controlManager = _controlManager;

	if(mapper) {
		shared_ptr<IBarcodeReader> barcodeReader = std::dynamic_pointer_cast<IBarcodeReader>(mapper->GetMapperControlDevice());
		if(barcodeReader) {
			barcodeReader->InputBarcode(barcode, digitCount);
		}
	}

	if(controlManager) {
		shared_ptr<IBarcodeReader> barcodeReader = std::dynamic_pointer_cast<IBarcodeReader>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort));
		if(barcodeReader) {
			barcodeReader->InputBarcode(barcode, digitCount);
		}
	}
}

bool Console::IsNsf()
{
	return std::dynamic_pointer_cast<NsfMapper>(_mapper) != nullptr;
}

uint8_t* Console::GetRamBuffer(uint16_t address)
{
	//Only used by libretro port for achievements - should not be used by anything else.
	AddressTypeInfo addrInfo;
	_mapper->GetAbsoluteAddressAndType(address, &addrInfo);
	
	if(addrInfo.Address >= 0)
	{
		if(addrInfo.Type == AddressType::InternalRam)
			return _memoryManager->GetInternalRAM() + addrInfo.Address;
		else if(addrInfo.Type == AddressType::WorkRam)
			return _mapper->GetWorkRam() + addrInfo.Address;
		else if(addrInfo.Type == AddressType::SaveRam)
			return _mapper->GetSaveRam() + addrInfo.Address;
	}
	return nullptr;
}
