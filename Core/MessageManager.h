#pragma once

#include "stdafx.h"

#include "IMessageManager.h"
#include <unordered_map>
#include "../Utilities/SimpleLock.h"

class MessageManager
{
private:
	static IMessageManager* _messageManager;
	static std::unordered_map<string, string> _enResources;

	static SimpleLock _logLock;
	static SimpleLock _messageLock;
	static std::list<string> _log;
	
public:
	static void RegisterMessageManager(IMessageManager* messageManager);
	static void UnregisterMessageManager(IMessageManager* messageManager);
	static void DisplayMessage(string title, string message, string param1 = "", string param2 = "");

	static void Log(string message = "");
	static string GetLog();
};
