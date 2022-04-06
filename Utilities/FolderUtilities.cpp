#include "stdafx.h"

#include <unordered_set>
#include <algorithm>
#include "FolderUtilities.h"
#include "UTF8Util.h"

string FolderUtilities::_homeFolder = "";
string FolderUtilities::_saveFolderOverride = "";
string FolderUtilities::_saveStateFolderOverride = "";
string FolderUtilities::_screenshotFolderOverride = "";
vector<string> FolderUtilities::_gameFolders = vector<string>();

void FolderUtilities::SetHomeFolder(string homeFolder)
{
	_homeFolder = homeFolder;
	CreateFolder(homeFolder);
}

string FolderUtilities::GetHomeFolder()
{
	if(_homeFolder.size() == 0) {
		throw std::runtime_error("Home folder not specified");
	}
	return _homeFolder;
}

void FolderUtilities::AddKnownGameFolder(string gameFolder)
{
	bool alreadyExists = false;
	string lowerCaseFolder = gameFolder;
	std::transform(lowerCaseFolder.begin(), lowerCaseFolder.end(), lowerCaseFolder.begin(), ::tolower);

	for(string folder : _gameFolders) {
		std::transform(folder.begin(), folder.end(), folder.begin(), ::tolower);
		if(folder.compare(lowerCaseFolder) == 0) {
			alreadyExists = true;
			break;
		}
	}

	if(!alreadyExists) {
		_gameFolders.push_back(gameFolder);
	}
}

vector<string> FolderUtilities::GetKnownGameFolders()
{
	return _gameFolders;
}

void FolderUtilities::SetFolderOverrides(string saveFolder, string saveStateFolder, string screenshotFolder)
{
	_saveFolderOverride = saveFolder;
	_saveStateFolderOverride = saveStateFolder;
	_screenshotFolderOverride = screenshotFolder;
}

string FolderUtilities::GetSaveFolder()
{
	string folder;
	if(_saveFolderOverride.empty()) {
		folder = CombinePath(GetHomeFolder(), "Saves");
	} else {
		folder = _saveFolderOverride;
	}
	CreateFolder(folder);
	return folder;
}

string FolderUtilities::GetHdPackFolder()
{
	string folder = CombinePath(GetHomeFolder(), "HdPacks");
	CreateFolder(folder);
	return folder;
}

string FolderUtilities::GetSaveStateFolder()
{
	string folder;
	if(_saveStateFolderOverride.empty()) {
		folder = CombinePath(GetHomeFolder(), "SaveStates");
	} else {
		folder = _saveStateFolderOverride;
	}
	CreateFolder(folder);
	return folder;
}

//Libretro: Avoid using filesystem API.

#ifdef _WIN32
static const char* PATHSEPARATOR = "\\";
#else 
static const char* PATHSEPARATOR = "/";
#endif

void FolderUtilities::CreateFolder(string folder)
{
}

vector<string> FolderUtilities::GetFolders(string rootFolder)
{
	return vector<string>();
}

vector<string> FolderUtilities::GetFilesInFolder(string rootFolder, std::unordered_set<string> extensions, bool recursive)
{
	return vector<string>();
}

string FolderUtilities::GetFilename(string filepath, bool includeExtension)
{
	size_t index = filepath.find_last_of(PATHSEPARATOR);
	string filename = (index == std::string::basic_string::npos) ? filepath : filepath.substr(index + 1);
	if(!includeExtension) {
		filename = filename.substr(0, filename.find_last_of("."));
	}
	return filename;
}

string FolderUtilities::GetFolderName(string filepath)
{
	size_t index = filepath.find_last_of(PATHSEPARATOR);
	return filepath.substr(0, index);
}

string FolderUtilities::CombinePath(string folder, string filename)
{
	if(folder.find_last_of(PATHSEPARATOR) != folder.length() - 1) {
		folder += PATHSEPARATOR;
	}
	return folder + filename;
}
