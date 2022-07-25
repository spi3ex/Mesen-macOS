#pragma once
#include "stdafx.h"
#include <memory>

class MemoryManager;
class Console;
class BaseMapper;
class VirtualFile;
struct RomData;

class MapperFactory
{
	private:
		static BaseMapper* GetMapperFromID(RomData &romData);

	public:
		static constexpr uint16_t FdsMapperID = 65535;
		static constexpr uint16_t StudyBoxMapperID = 65533;

		static std::shared_ptr<BaseMapper> InitializeFromFile(std::shared_ptr<Console> console, VirtualFile &romFile, RomData &outRomData);
};
