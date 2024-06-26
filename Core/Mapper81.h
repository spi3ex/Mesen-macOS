#pragma once
#include "stdafx.h"
#include "BaseMapper.h"

class Mapper81 : public BaseMapper
{
protected:
	virtual uint16_t GetPRGPageSize() override { return 0x4000; }
	virtual uint16_t GetCHRPageSize() override { return 0x2000; }

	void InitMapper() override
	{
		SelectPRGPage(0, 0);
		SelectPRGPage(1, -1);
        SelectCHRPage(0, 0);
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		SelectPRGPage(0, addr >> 2);
		SelectCHRPage(0, addr & 0x03);
	}
};
