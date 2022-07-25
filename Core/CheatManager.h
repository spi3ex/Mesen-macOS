#pragma once

#include "stdafx.h"
#include <memory>

class Console;

struct CodeInfo
{
	uint32_t Address;
	uint8_t Value;
	int32_t CompareValue;
	bool IsRelativeAddress;
};

enum class CheatType
{
	GameGenie = 0,
	ProActionRocky = 1,
	Custom = 2
};

struct CheatInfo
{
	CheatType Type;
	uint32_t ProActionRockyCode;
	uint32_t Address;
	char GameGenieCode[9];
	uint8_t Value;
	uint8_t CompareValue;
	bool UseCompareValue;
	bool IsRelativeAddress;
};

class CheatManager
{
private:
	std::shared_ptr<Console> _console;

	bool _hasCode = false;
	vector<std::unique_ptr<vector<CodeInfo>>> _relativeCheatCodes;
	vector<CodeInfo> _absoluteCheatCodes;

	uint32_t DecodeValue(uint32_t code, uint32_t* bitIndexes, uint32_t bitCount);
	CodeInfo GetGGCodeInfo(string ggCode);
	CodeInfo GetPARCodeInfo(uint32_t parCode);
	void AddCode(CodeInfo &code);
	
public:
	CheatManager(std::shared_ptr<Console> console);

	void AddGameGenieCode(string code);
	void AddProActionRockyCode(uint32_t code);
	void AddCustomCode(uint32_t address, uint8_t value, int32_t compareValue = -1, bool isRelativeAddress = true);
	void ClearCodes();

	void ApplyCodes(uint16_t addr, uint8_t &value);
};
