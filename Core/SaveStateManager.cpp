#include "stdafx.h"
#include "SaveStateManager.h"
#include "MessageManager.h"
#include "Console.h"
#include "EmulationSettings.h"
#include "VideoDecoder.h"
#include "Debugger.h"
#include "MovieManager.h"
#include "RomData.h"
#include "DefaultVideoFilter.h"
#include "PPU.h"

SaveStateManager::SaveStateManager(shared_ptr<Console> console)
{
	_console = console;
}

void SaveStateManager::GetSaveStateHeader(ostream &stream)
{
	uint32_t emuVersion = EmulationSettings::GetMesenVersion();
	uint32_t formatVersion = SaveStateManager::FileFormatVersion;
	stream.write("MST", 3);
	stream.write((char*)&emuVersion, sizeof(emuVersion));
	stream.write((char*)&formatVersion, sizeof(uint32_t));

	RomInfo romInfo = _console->GetRomInfo();
	stream.write((char*)&romInfo.MapperID, sizeof(uint16_t));
	stream.write((char*)&romInfo.SubMapperID, sizeof(uint8_t));

	string sha1Hash = romInfo.Hash.Sha1;
	stream.write(sha1Hash.c_str(), sha1Hash.size());

	string romName = romInfo.RomName;
	uint32_t nameLength = (uint32_t)romName.size();
	stream.write((char*)&nameLength, sizeof(uint32_t));
	stream.write(romName.c_str(), romName.size());
}

void SaveStateManager::SaveState(ostream &stream)
{
	GetSaveStateHeader(stream);
	_console->SaveState(stream);
}

bool SaveStateManager::LoadState(istream &stream, bool hashCheckRequired)
{
	char header[3];
	stream.read(header, 3);
	if(memcmp(header, "MST", 3) == 0) {
		uint32_t emuVersion, fileFormatVersion;

		stream.read((char*)&emuVersion, sizeof(emuVersion));
		if(emuVersion > EmulationSettings::GetMesenVersion()) {
			MessageManager::DisplayMessage("SaveStates", "SaveStateNewerVersion");
			return false;
		}

		stream.read((char*)&fileFormatVersion, sizeof(fileFormatVersion));
		if(fileFormatVersion <= 11) {
			MessageManager::DisplayMessage("SaveStates", "SaveStateIncompatibleVersion");
			return false;
		} else {
			int32_t mapperId = -1;
			int32_t subMapperId = -1;
			uint16_t id;
			uint8_t sid;
			stream.read((char*)&id, sizeof(uint16_t));
			stream.read((char*)&sid, sizeof(uint8_t));
			mapperId = id;
			subMapperId = sid;

			char hash[41] = {};
			stream.read(hash, 40);

			uint32_t nameLength = 0;
			stream.read((char*)&nameLength, sizeof(uint32_t));
			
			vector<char> nameBuffer(nameLength);
			stream.read(nameBuffer.data(), nameBuffer.size());
			string romName(nameBuffer.data(), nameLength);
			
			RomInfo romInfo = _console->GetRomInfo();
			bool gameLoaded = !romInfo.Hash.Sha1.empty();
			if(romInfo.Hash.Sha1 != string(hash)) {
				//CRC doesn't match
				if(!_console->GetSettings()->CheckFlag(EmulationFlags::AllowMismatchingSaveState) || !gameLoaded ||
					romInfo.MapperID != mapperId || romInfo.SubMapperID != subMapperId)
				{
					//If mismatching states aren't allowed, or a game isn't loaded, or the mapper types don't match, try to find and load the matching ROM
					HashInfo info;
					info.Sha1 = hash;
					if(!_console->LoadMatchingRom(romName, info)) {
						MessageManager::DisplayMessage("SaveStates", "SaveStateMissingRom", romName);
						return false;
					}
				}
			}
		}

		//Stop any movie that might have been playing/recording if a state is loaded
		//(Note: Loading a state is disabled in the UI while a movie is playing/recording)
		MovieManager::Stop();

		_console->LoadState(stream, fileFormatVersion);

		return true;
	}
	MessageManager::DisplayMessage("SaveStates", "SaveStateInvalidFile");
	return false;
}
