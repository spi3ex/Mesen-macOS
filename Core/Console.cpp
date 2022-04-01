#include "stdafx.h"
#include <random>
#include <thread>
#include "Console.h"
#include "CPU.h"
#include "PPU.h"
#include "APU.h"
#include "MemoryManager.h"
#include "BaseMapper.h"
#include "ControlManager.h"
#include "VsControlManager.h"
#include "MapperFactory.h"
#include "Debugger.h"
#include "MessageManager.h"
#include "EmulationSettings.h"
#include "../Utilities/sha1.h"
#include "../Utilities/Timer.h"
#include "../Utilities/FolderUtilities.h"
#include "VirtualFile.h"
#include "HdBuilderPpu.h"
#include "HdPpu.h"
#include "NsfPpu.h"
#include "SoundMixer.h"
#include "NsfMapper.h"
#include "MovieManager.h"
#include "RewindManager.h"
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
#include "NotificationManager.h"
#include "HistoryViewer.h"
#include "ConsolePauseHelper.h"
#include "EventManager.h"

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

	_pauseCounter = 0;
	_model = NesModel::NTSC;
}

Console::~Console()
{
	MovieManager::Stop();
}

void Console::Init()
{
	_notificationManager.reset(new NotificationManager());
	_batteryManager.reset(new BatteryManager());
	
	_videoRenderer.reset(new VideoRenderer());
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
		_notificationManager.reset();
	}

	if(_master) {
		_master->_notificationManager->SendNotification(ConsoleNotificationType::VsDualSystemStopped);
	}

	_rewindManager.reset();

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
		Pause();

		if(!_romFilepath.empty() && _mapper) {
			//Ensure we save any battery file before loading a new game
			SaveBatteries();
		}

		shared_ptr<HdPackData> originalHdPackData = _hdData;
		LoadHdPack(romFile, patchFile);
		if(patchFile.IsValid()) {
			if(romFile.ApplyPatch(patchFile)) {
				MessageManager::DisplayMessage("Patch", "ApplyingPatch", patchFile.GetFileName());
			} else {
				//Patch failed
			}
		}

		_batteryManager->Initialize(FolderUtilities::GetFilename(romFile.GetFileName(), false));

		RomData romData;
		shared_ptr<BaseMapper> mapper = MapperFactory::InitializeFromFile(shared_from_this(), romFile, romData);
		if(mapper) {
			bool isDifferentGame = _romFilepath != (string)romFile || _patchFilename != (string)patchFile;
			if(_mapper) {
				//Send notification only if a game was already running and we successfully loaded the new one
				_notificationManager->SendNotification(ConsoleNotificationType::GameStopped, (void*)1);
			}

			if(isDifferentGame) {
				_romFilepath = romFile;
				_patchFilename = patchFile;
				
				//Changed game, stop all recordings
				MovieManager::Stop();
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
				_slave.reset(new Console(shared_from_this()));
				_slave->Init();
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
			} else if(std::dynamic_pointer_cast<NsfMapper>(_mapper)) {
				//Disable most of the PPU for NSFs
				_ppu.reset(new NsfPpu(shared_from_this()));
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

			//Reset components before creating rewindmanager, otherwise the first save state it takes will be invalid
			if(!forPowerCycle) {
				KeyManager::UpdateDevices();
				_rewindManager.reset(new RewindManager(shared_from_this()));
				_notificationManager->RegisterNotificationListener(_rewindManager);
			} else {
				_rewindManager->Initialize();
			}

			//Poll controller input after creating rewind manager, to make sure it catches the first frame's input
			_controlManager->UpdateInputState();

			FolderUtilities::AddKnownGameFolder(romFile.GetFolderPath());

			if(IsMaster()) {
				if(!forPowerCycle) {
					string modelName = _model == NesModel::PAL ? "PAL" : (_model == NesModel::Dendy ? "Dendy" : "NTSC");
					string messageTitle = MessageManager::Localize("GameLoaded") + " (" + modelName + ")";
					MessageManager::DisplayMessage(messageTitle, FolderUtilities::GetFilename(GetRomInfo().RomName, false));
				}

				_settings->ClearFlags(EmulationFlags::ForceMaxSpeed);

				if(_slave) {
					_notificationManager->SendNotification(ConsoleNotificationType::VsDualSystemStarted);
				}
			}

			//Used by netplay to take save state after UpdateInputState() is called above, to ensure client+server are in sync
			if(!_master) {
				_notificationManager->SendNotification(ConsoleNotificationType::GameInitCompleted);
			}

			Resume();
			return true;
		} else {
			_hdData = originalHdPackData;

			//Reset battery source to current game if new game failed to load
			_batteryManager->Initialize(FolderUtilities::GetFilename(GetRomInfo().RomName, false));

			Resume();
		}
	}

	MessageManager::DisplayMessage("Error", "CouldNotLoadFile", romFile.GetFileName());
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

shared_ptr<NotificationManager> Console::GetNotificationManager()
{
	return _notificationManager;
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

shared_ptr<RewindManager> Console::GetRewindManager()
{
	return _rewindManager;
}

HistoryViewer* Console::GetHistoryViewer()
{
	return _historyViewer.get();
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
		bool needSuspend = softReset ? _systemActionManager->Reset() : _systemActionManager->PowerCycle();
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

	_resetRunTimers = true;

	//This notification MUST be sent before the UpdateInputState() below to allow MovieRecorder to grab the first frame's worth of inputs
	if(!_master) {
		_notificationManager->SendNotification(softReset ? ConsoleNotificationType::GameReset : ConsoleNotificationType::GameLoaded);
	}
}

void Console::Stop(int stopCode)
{
	_stop = true;
	_stopCode = stopCode;

	_stopLock.Acquire();
	_stopLock.Release();
}

void Console::Pause()
{
	if(_master) {
		//When trying to pause/resume the slave, we need to pause/resume the master instead
		_master->Pause();
	} else {
		_pauseCounter++;
		_runLock.Acquire();
	}
}

void Console::Resume()
{
	if(_master) {
		//When trying to pause/resume the slave, we need to pause/resume the master instead
		_master->Resume();
	} else {
		_runLock.Release();
		_pauseCounter--;
	}
}

void Console::RunSingleFrame()
{
	//Used by Libretro
	uint32_t lastFrameNumber = _ppu->GetFrameCount();
	UpdateNesModel(true);

	while(_ppu->GetFrameCount() == lastFrameNumber) {
		_cpu->Exec();
		if(_slave) {
			RunSlaveCpu();
		}
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

void Console::RunFrame()
{
	uint32_t frameCount = _ppu->GetFrameCount();
	while(_ppu->GetFrameCount() == frameCount) {
		_cpu->Exec();
		if(_slave) {
			RunSlaveCpu();
		}
	}
}

void Console::Run()
{
	Timer clockTimer;
	Timer lastFrameTimer;
	double targetTime;
	double lastDelay = GetFrameDelay();

	_runLock.Acquire();
	_stopLock.Acquire();

	targetTime = lastDelay;

	UpdateNesModel(true);

	_running = true;

	try {
		while(true) {
			stringstream runAheadState;
			bool useRunAhead = _settings->GetRunAheadFrames() > 0 && !IsNsf() && !_rewindManager->IsRewinding() && _settings->GetEmulationSpeed() > 0 && _settings->GetEmulationSpeed() <= 100;
			if(useRunAhead) {
				RunFrameWithRunAhead(runAheadState);
			} else {
				RunFrame();
			}

			if(_historyViewer) {
				_historyViewer->ProcessEndOfFrame();
			}
			_rewindManager->ProcessEndOfFrame();
			_settings->DisableOverclocking(_disableOcNextFrame || IsNsf());
			_disableOcNextFrame = false;

			//Update model (ntsc/pal) and get delay for next frame
			UpdateNesModel(true);
			double delay = GetFrameDelay();

			if(_resetRunTimers || delay != lastDelay || (clockTimer.GetElapsedMS() - targetTime) > 300) {
				//Reset the timers, this can happen in 3 scenarios:
				//1) Target frame rate changed
				//2) The console was reset/power cycled or the emulation was paused (with or without the debugger)
				//3) As a satefy net, if we overshoot our target by over 300 milliseconds, the timer is reset, too.
				//   This can happen when something slows the emulator down severely (or when breaking execution in VS when debugging Mesen itself, etc.)
				clockTimer.Reset();
				targetTime = 0;

				_resetRunTimers = false;
				lastDelay = delay;
			}

			targetTime += delay;
				
			//When sleeping for a long time (e.g <= 25% speed), sleep in small chunks and check to see if we need to stop sleeping between each sleep call
			while(targetTime - clockTimer.GetElapsedMS() > 50) {
				clockTimer.WaitUntil(clockTimer.GetElapsedMS() + 40);
				if(delay != GetFrameDelay() || _stop || _settings->NeedsPause() || _pauseCounter > 0) {
					targetTime = 0;
					break;
				}
			}

			//Sleep until we're ready to start the next frame
			clockTimer.WaitUntil(targetTime);

			if(useRunAhead) {
				_settings->SetRunAheadFrameFlag(true);
				LoadState(runAheadState);
				_settings->SetRunAheadFrameFlag(false);
			}

			if(_pauseCounter > 0) {
				//Need to temporarely pause the emu (to save/load a state, etc.)
				_runLock.Release();

				//Spin wait until we are allowed to start again
				while(_pauseCounter > 0) { }

				_runLock.Acquire();
			}

			if(_pauseOnNextFrameRequested) {
				//Used by "Run Single Frame" option
				_settings->SetFlags(EmulationFlags::Paused);
				_pauseOnNextFrameRequested = false;
			}

			bool pausedRequired = _settings->NeedsPause();
			if(pausedRequired && !_stop && !_settings->CheckFlag(EmulationFlags::DebuggerWindowEnabled)) {
				_notificationManager->SendNotification(ConsoleNotificationType::GamePaused);

				_runLock.Release();

				while(pausedRequired && !_stop && !_settings->CheckFlag(EmulationFlags::DebuggerWindowEnabled)) {
					//Sleep until emulation is resumed
					std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(30));
					pausedRequired = _settings->NeedsPause();
					_paused = true;
				}
				_paused = false;
					
				_runLock.Acquire();
				_notificationManager->SendNotification(ConsoleNotificationType::GameResumed);
				lastFrameTimer.Reset();

				//Reset the timer to avoid speed up after a pause
				_resetRunTimers = true;
			}

			_systemActionManager->ProcessSystemActions();

			if(_stop) {
				_stop = false;
				break;
			}
		}
	} catch(const std::runtime_error &ex) {
		_stopCode = -1;
		MessageManager::DisplayMessage("Error", "GameCrash", ex.what());
	}

	_paused = false;
	_running = false;

	_notificationManager->SendNotification(ConsoleNotificationType::BeforeEmulationStop);

	StopRecordingHdPack();

	MovieManager::Stop();
	_soundMixer->StopRecording();

	_settings->ClearFlags(EmulationFlags::ForceMaxSpeed);

	_initialized = false;

	if(!_romFilepath.empty() && _mapper) {
		//Ensure we save any battery file before unloading anything
		SaveBatteries();
	}

	_romFilepath = "";

	Release(false);
	
	_stopLock.Release();
	_runLock.Release();

	_notificationManager->SendNotification(ConsoleNotificationType::GameStopped);
	_notificationManager->SendNotification(ConsoleNotificationType::EmulationStopped);
}

void Console::RunFrameWithRunAhead(std::stringstream& runAheadState)
{
	uint32_t runAheadFrames = _settings->GetRunAheadFrames();
	_settings->SetRunAheadFrameFlag(true);
	//Run a single frame and save the state (no audio/video)
	RunFrame();
	SaveState(runAheadState);
	while(runAheadFrames > 1) {
		//Run extra frames if the requested run ahead frame count is higher than 1
		runAheadFrames--;
		RunFrame();
	}
	_apu->EndFrame();
	_settings->SetRunAheadFrameFlag(false);

	//Run one frame normally (with audio/video output)
	RunFrame();
	_apu->EndFrame();
}

void Console::ResetRunTimers()
{
	_resetRunTimers = true;
}

bool Console::IsRunning()
{
	//For slave CPU, return the master's state
	if(_master)
		return _master->IsRunning();
	return !_stopLock.IsFree() && _running;
}

bool Console::IsExecutionStopped()
{
	//For slave CPU, return the master's state
	if(_master)
		return _master->IsPaused();
	return _runLock.IsFree() || (!_runLock.IsFree() && _pauseCounter > 0) || !_running;
}

bool Console::IsPaused()
{
	if(_master)
		return _master->_paused;
	return _paused;
}

void Console::PauseOnNextFrame()
{
	_pauseOnNextFrameRequested = true;
}

void Console::UpdateNesModel(bool sendNotification)
{
	bool configChanged = false;
	if(_settings->NeedControllerUpdate()) {
		_controlManager->UpdateControlDevices();
		configChanged = true;
	}

	NesModel model = _settings->GetNesModel();
	if(model == NesModel::Auto) {
		switch(_mapper->GetRomInfo().System) {
			case GameSystem::NesPal: model = NesModel::PAL; break;
			case GameSystem::Dendy: model = NesModel::Dendy; break;
			default: model = NesModel::NTSC; break;
		}
	}
	if(_model != model) {
		_model = model;
		configChanged = true;

		if(sendNotification) {
			MessageManager::DisplayMessage("Region", model == NesModel::PAL ? "PAL" : (model == NesModel::Dendy ? "Dendy" : "NTSC"));
		}
	}

	_cpu->SetMasterClockDivider(model);
	_mapper->SetNesModel(model);
	_ppu->SetNesModel(model);
	_apu->SetNesModel(model);

	if(configChanged && sendNotification) {
		_notificationManager->SendNotification(ConsoleNotificationType::ConfigChanged);
	}
}

double Console::GetFrameDelay()
{
	uint32_t emulationSpeed = _settings->GetEmulationSpeed();
	double frameDelay;
	if(emulationSpeed == 0) {
		frameDelay = 0;
	} else {
		//60.1fps (NTSC), 50.01fps (PAL/Dendy)
		switch(_model) {
			default:
			case NesModel::NTSC: frameDelay = _settings->CheckFlag(EmulationFlags::IntegerFpsMode) ? 16.6666666666666666667 : 16.63926405550947; break;
			case NesModel::PAL:
			case NesModel::Dendy: frameDelay = _settings->CheckFlag(EmulationFlags::IntegerFpsMode) ? 20 : 19.99720920217466; break;
		}
		frameDelay /= (double)emulationSpeed / 100.0;
	}

	return frameDelay;
}

double Console::GetFps()
{
	if(_model == NesModel::NTSC)
		return _settings->CheckFlag(EmulationFlags::IntegerFpsMode) ? 60.0 : 60.098812;
	return _settings->CheckFlag(EmulationFlags::IntegerFpsMode) ? 50.0 : 50.006978;
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
		
		_notificationManager->SendNotification(ConsoleNotificationType::StateLoaded);
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

uint32_t Console::GetLagCounter()
{
	return _controlManager->GetLagCounter();
}

void Console::ResetLagCounter()
{
	Pause();
	_controlManager->ResetLagCounter();
	Resume();
}

bool Console::IsDebuggerAttached()
{
	return false;
}

void Console::SetNextFrameOverclockStatus(bool disabled)
{
	_disableOcNextFrame = disabled;
}

int32_t Console::GetStopCode()
{
	return _stopCode;
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
	ConsolePauseHelper helper(this);

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
		ConsolePauseHelper helper(this);

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
	Pause();

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
		} else if(std::dynamic_pointer_cast<NsfMapper>(_mapper)) {
			//Disable most of the PPU for NSFs
			_ppu.reset(new NsfPpu(shared_from_this()));
		} else {
			_ppu.reset(new PPU(shared_from_this()));
		}
		_memoryManager->RegisterIODevice(_ppu.get());

		LoadState(saveState);
		modeChanged = true;
	}

	Resume();
	
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

void Console::LoadTapeFile(string filepath)
{
	shared_ptr<ControlManager> controlManager = _controlManager;
	if(controlManager) {
		shared_ptr<FamilyBasicDataRecorder> dataRecorder = std::dynamic_pointer_cast<FamilyBasicDataRecorder>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort2));
		if(dataRecorder) {
			Pause();
			dataRecorder->LoadFromFile(filepath);
			Resume();
		}
	}
}

void Console::StartRecordingTapeFile(string filepath)
{
	shared_ptr<ControlManager> controlManager = _controlManager;
	if(controlManager) {
		shared_ptr<FamilyBasicDataRecorder> dataRecorder = std::dynamic_pointer_cast<FamilyBasicDataRecorder>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort2));
		if(dataRecorder) {
			Pause();
			dataRecorder->StartRecording(filepath);
			Resume();
		}
	}
}

void Console::StopRecordingTapeFile()
{
	shared_ptr<ControlManager> controlManager = _controlManager;
	if(controlManager) {
		shared_ptr<FamilyBasicDataRecorder> dataRecorder = std::dynamic_pointer_cast<FamilyBasicDataRecorder>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort2));
		if(dataRecorder) {
			Pause();
			dataRecorder->StopRecording();
			Resume();
		}
	}
}

bool Console::IsRecordingTapeFile()
{
	shared_ptr<ControlManager> controlManager = _controlManager;
	if(controlManager) {
		shared_ptr<FamilyBasicDataRecorder> dataRecorder = std::dynamic_pointer_cast<FamilyBasicDataRecorder>(controlManager->GetControlDevice(BaseControlDevice::ExpDevicePort2));
		if(dataRecorder) {
			return dataRecorder->IsRecording();
		}
	}

	return false;
}

bool Console::IsNsf()
{
	return std::dynamic_pointer_cast<NsfMapper>(_mapper) != nullptr;
}

void Console::CopyRewindData(shared_ptr<Console> sourceConsole)
{
	sourceConsole->Pause();
	Pause();

	//Disable battery saving for this instance
	_batteryManager->SetSaveEnabled(false);
	_historyViewer.reset(new HistoryViewer(shared_from_this()));
	sourceConsole->_rewindManager->CopyHistory(_historyViewer);

	Resume();
	sourceConsole->Resume();
}

uint8_t* Console::GetRamBuffer(uint16_t address)
{
	//Only used by libretro port for achievements - should not be used by anything else.
	AddressTypeInfo addrInfo;
	_mapper->GetAbsoluteAddressAndType(address, &addrInfo);
	
	if(addrInfo.Address >= 0) {
		if(addrInfo.Type == AddressType::InternalRam) {
			return _memoryManager->GetInternalRAM() + addrInfo.Address;
		} else if(addrInfo.Type == AddressType::WorkRam) {
			return _mapper->GetWorkRam() + addrInfo.Address;
		} else if(addrInfo.Type == AddressType::SaveRam) {
			return _mapper->GetSaveRam() + addrInfo.Address;
		}
	}
	return nullptr;
}

void Console::DebugAddTrace(const char * log)
{
}

void Console::DebugProcessPpuCycle()
{
}

void Console::DebugProcessEvent(EventType type)
{
}

void Console::DebugProcessInterrupt(uint16_t cpuAddr, uint16_t destCpuAddr, bool forNmi)
{
}

void Console::DebugSetLastFramePpuScroll(uint16_t addr, uint8_t xScroll, bool updateHorizontalScrollOnly)
{
}

void Console::DebugAddDebugEvent(DebugEventType type)
{
}

bool Console::DebugProcessRamOperation(MemoryOperationType type, uint16_t & addr, uint8_t & value)
{
	return true;
}

void Console::DebugProcessVramReadOperation(MemoryOperationType type, uint16_t addr, uint8_t & value)
{
}

void Console::DebugProcessVramWriteOperation(uint16_t addr, uint8_t & value)
{
}
