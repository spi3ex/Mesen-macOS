#pragma once 
#include "stdafx.h"
#ifdef HAVE_TLS
#include <thread>
#endif

class SimpleLock;

class LockHandler
{
private:
	SimpleLock *_lock;
public:
	LockHandler(SimpleLock *lock);
	~LockHandler();
};

class SimpleLock
{
private:
#ifdef HAVE_TLS
	thread_local static std::thread::id _threadID;
#else
   uint32_t _holderThreadID;
#endif

	std::thread::id _holderThreadID;
	uint32_t _lockCount;
	atomic_flag _lock;

public:
	SimpleLock();
	~SimpleLock();

	LockHandler AcquireSafe();

	void Acquire();
	bool IsFree();
	void WaitForRelease();
	void Release();
};

