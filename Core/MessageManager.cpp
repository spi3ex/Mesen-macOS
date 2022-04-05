#include "stdafx.h"
#include "MessageManager.h"
#include "EmulationSettings.h"

std::unordered_map<string, string> MessageManager::_enResources = {
	{ "Cheats", u8"Cheats" },
	{ "Debug", u8"Debug" },
	{ "EmulationSpeed", u8"Emulation Speed" },
	{ "ClockRate", u8"Clock Rate" },
	{ "Error", u8"Error" },
	{ "GameInfo", u8"Game Info" },
	{ "GameLoaded", u8"Game loaded" },
	{ "Input", u8"Input" },
	{ "Patch", u8"Patch" },
	{ "Movies", u8"Movies" },
	{ "NetPlay", u8"Net Play" },
	{ "Region", u8"Region" },
	{ "SaveStates", u8"Save States" },
	{ "ScreenshotSaved", u8"Screenshot Saved" },
	{ "SoundRecorder", u8"Sound Recorder" },
	{ "Test", u8"Test" },
	{ "VideoRecorder", u8"Video Recorder" },

	{ "ApplyingPatch", u8"Applying patch: %1" },
	{ "CheatApplied", u8"1 cheat applied." },
	{ "CheatsApplied", u8"%1 cheats applied." },
	{ "CheatsDisabled", u8"All cheats disabled." },
	{ "CoinInsertedSlot", u8"Coin inserted (slot %1)" },
	{ "ConnectedToServer", u8"Connected to server." },
	{ "ConnectedAsPlayer", u8"Connected as player %1" },
	{ "ConnectedAsSpectator", u8"Connected as spectator." },
	{ "ConnectionLost", u8"Connection to server lost." },
	{ "CouldNotConnect", u8"Could not connect to the server." },
	{ "CouldNotInitializeAudioSystem", u8"Could not initialize audio system" },
	{ "CouldNotFindRom", u8"Could not find matching game ROM." },
	{ "CouldNotLoadFile", u8"Could not load file: %1" },
	{ "EmulationMaximumSpeed", u8"Maximum speed" },
	{ "EmulationSpeedPercent", u8"%1%" },
	{ "FdsDiskInserted", u8"Disk %1 Side %2 inserted." },
	{ "Frame", u8"Frame" },
	{ "GameCrash", u8"Game has crashed (%1)" },
	{ "KeyboardModeDisabled", u8"Keyboard mode disabled." },
	{ "KeyboardModeEnabled", u8"Keyboard mode enabled." },
	{ "Lag", u8"Lag" },
	{ "Mapper", u8"Mapper: %1, SubMapper: %2" },
	{ "MovieEnded", u8"Movie ended." },
	{ "MovieInvalid", u8"Invalid movie file." },
	{ "MovieMissingRom", u8"Missing ROM required (%1) to play movie." },
	{ "MovieNewerVersion", u8"Cannot load movies created by a more recent version of Mesen. Please download the latest version." },
	{ "MovieIncompatibleVersion", u8"This movie is incompatible with this version of Mesen." },
	{ "MoviePlaying", u8"Playing movie: %1" },
	{ "MovieRecordingTo", u8"Recording to: %1" },
	{ "MovieSaved", u8"Movie saved to file: %1" },
	{ "NetplayVersionMismatch", u8"%1 is not running the same version of Mesen and has been disconnected." },
	{ "PrgSizeWarning", u8"PRG size is smaller than 32kb" },
	{ "SaveStateEmpty", u8"Slot is empty." },
	{ "SaveStateIncompatibleVersion", u8"Save state is incompatible with this version of Mesen." },
	{ "SaveStateInvalidFile", u8"Invalid save state file." },
	{ "SaveStateLoaded", u8"State #%1 loaded." },
	{ "SaveStateMissingRom", u8"Missing ROM required (%1) to load save state." },
	{ "SaveStateNewerVersion", u8"Cannot load save states created by a more recent version of Mesen. Please download the latest version." },
	{ "SaveStateSaved", u8"State #%1 saved." },
	{ "SaveStateSlotSelected", u8"Slot #%1 selected." },
	{ "ScanlineTimingWarning", u8"PPU timing has been changed." },
	{ "ServerStarted", u8"Server started (Port: %1)" },
	{ "ServerStopped", u8"Server stopped" },
	{ "SoundRecorderStarted", u8"Recording to: %1" },
	{ "SoundRecorderStopped", u8"Recording saved to: %1" },
	{ "TestFileSavedTo", u8"Test file saved to: %1" },
	{ "UnsupportedMapper", u8"Unsupported mapper (%1), cannot load game." },
	{ "VideoRecorderStarted", u8"Recording to: %1" },
	{ "VideoRecorderStopped", u8"Recording saved to: %1" },

	{ "GoogleDrive", u8"Google Drive" },
	{ "SynchronizationStarted", u8"Synchronization started." },
	{ "SynchronizationFailed", u8"Synchronization failed." },
	{ "SynchronizationCompleted", u8"Synchronization completed." },
};

std::list<string> MessageManager::_log;
IMessageManager* MessageManager::_messageManager = nullptr;

void MessageManager::RegisterMessageManager(IMessageManager* messageManager)
{
	MessageManager::_messageManager = messageManager;
}

void MessageManager::UnregisterMessageManager(IMessageManager* messageManager)
{
	if(MessageManager::_messageManager == messageManager) {
		MessageManager::_messageManager = nullptr;
	}
}

void MessageManager::DisplayMessage(string title, string message, string param1, string param2)
{
	if(MessageManager::_messageManager) {
		if(!MessageManager::_messageManager) {
			return;
		}

		size_t startPos = message.find(u8"%1");
		if(startPos != std::string::npos) {
			message.replace(startPos, 2, param1);
		}

		startPos = message.find(u8"%2");
		if(startPos != std::string::npos)
			message.replace(startPos, 2, param2);

		MessageManager::Log("[" + title + "] " + message);
	}
}

void MessageManager::Log(string message)
{
	if(MessageManager::_messageManager) {
		MessageManager::_messageManager->DisplayMessage("", message + "\n");
	}
}
