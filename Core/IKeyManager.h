#pragma once

#include "stdafx.h"

enum class MouseButton
{
	LeftButton = 0,
	RightButton = 1,
	MiddleButton = 2,
};

class IKeyManager
{
public:
	virtual ~IKeyManager() {}

	virtual void RefreshState() = 0;
	virtual void UpdateDevices() = 0;
	virtual bool IsMouseButtonPressed(MouseButton button) = 0;
	virtual bool IsKeyPressed(uint32_t keyCode) = 0;
	virtual vector<uint32_t> GetPressedKeys() = 0;
	virtual string GetKeyName(uint32_t keyCode) = 0;
	virtual uint32_t GetKeyCode(string keyName) = 0;
};
