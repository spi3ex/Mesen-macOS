#pragma once

#include "stdafx.h"

#include "IMessageManager.h"
#include <unordered_map>

class MessageManager
{
private:
	static IMessageManager* _messageManager;

	static std::list<string> _log;
	
public:
	static void RegisterMessageManager(IMessageManager* messageManager);
	static void UnregisterMessageManager(IMessageManager* messageManager);
	static void DisplayMessage(string title, string message, string param1 = "", string param2 = "");

	static void Log(string message = "");
};
