#pragma once
#include "stdafx.h"
#include "Types.h"

class IKeyManager;
class EmulationSettings;
enum class MouseButton;

class KeyManager
{
private:
	static IKeyManager* _keyManager;
	static MousePosition _mousePosition;
	static atomic<int16_t> _xMouseMovement;
	static atomic<int16_t> _yMouseMovement;
	static EmulationSettings* _settings;

public:
	static void RegisterKeyManager(IKeyManager* keyManager);
	static void SetSettings(EmulationSettings* settings);

	static void RefreshKeyState();
	static bool IsKeyPressed(uint32_t keyCode);
	static bool IsMouseButtonPressed(MouseButton button);
	static string GetKeyName(uint32_t keyCode);
	static uint32_t GetKeyCode(string keyName);

	static void SetMouseMovement(int16_t x, int16_t y);
	static MouseMovement GetMouseMovement(double mouseSensitivity);
	
	static void SetMousePosition(double x, double y);
	static MousePosition GetMousePosition();
};
