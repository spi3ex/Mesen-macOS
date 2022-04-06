#pragma once

#include "stdafx.h"
#include "DebuggerTypes.h"

class Debugger;

enum class CdlPrgFlags
{
	None = 0x00,
	Code = 0x01,
	Data = 0x02,
	
	//Bit 0x10 is used for "indirectly accessed as code" in FCEUX
	//Repurposed to mean the address is the target of a jump instruction
	JumpTarget = 0x10,

	IndirectData = 0x20,
	PcmData = 0x40,

	//Unused bit in original CDL spec
	//Used to denote that the byte is the start of function (sub)
	SubEntryPoint = 0x80
};

enum class CdlChrFlags
{
	Drawn = 0x01,
	Read = 0x02,
};

enum class CdlStripFlag
{
	StripNone = 0,
	StripUnused,
	StripUsed,
};

struct CdlRatios
{
	float CodeRatio;
	float DataRatio;
	float PrgRatio;
	
	float ChrRatio;
	float ChrReadRatio;
	float ChrDrawnRatio;
};

class CodeDataLogger
{
private:
	Debugger* _debugger = nullptr;
	uint8_t *_cdlData = nullptr;
	uint32_t _prgSize = 0;
	uint32_t _chrSize = 0;

	uint32_t _codeSize = 0;
	uint32_t _dataSize = 0;
	uint32_t _usedChrSize = 0;
	uint32_t _readChrSize = 0;
	uint32_t _drawnChrSize = 0;

	void CalculateStats();

public:
	CodeDataLogger(Debugger *debugger, uint32_t prgSize, uint32_t chrSize);
	~CodeDataLogger();

	void Reset();

	void SetFlag(int32_t absoluteAddr, CdlPrgFlags flag);
	void SetFlag(int32_t chrAbsoluteAddr, CdlChrFlags flag);

	CdlRatios GetRatios();

	bool IsCode(uint32_t absoluteAddr);
	bool IsJumpTarget(uint32_t absoluteAddr);
	bool IsSubEntryPoint(uint32_t absoluteAddr);
	bool IsData(uint32_t absoluteAddr);
	bool IsRead(uint32_t absoluteAddr);
	bool IsDrawn(uint32_t absoluteAddr);
};
