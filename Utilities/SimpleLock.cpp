#include "stdafx.h"
#include <assert.h>
#include "SimpleLock.h"
#ifdef HAVE_TLS
thread_local std::thread::id SimpleLock::_threadID = std::this_thread::get_id();

#define GetThreadId() _threadID
#define GetThreadIdDefault() std::thread::id()
#else

#ifdef _WIN32
#include <windows.h>
#define GetThreadId() GetCurrentThreadId()
#else
#define GetThreadId() std::thread::id
#endif

#define GetThreadIdDefault() ~0

#endif

SimpleLock::SimpleLock()
{
	_lock.clear();
	_lockCount = 0;
   _holderThreadID = GetThreadIdDefault();
}

SimpleLock::~SimpleLock()
{
}

LockHandler SimpleLock::AcquireSafe()
{
	return LockHandler(this);
}

void SimpleLock::Acquire()
{
   uint32_t thread_id = GetThreadId();

	if(_lockCount == 0 || _holderThreadID != thread_id)
   {
      while(_lock.test_and_set());
      _holderThreadID = thread_id;
      _lockCount = 1;
   }
   else //Same thread can acquire the same lock multiple times
		_lockCount++;
}

bool SimpleLock::IsFree()
{
	return _lockCount == 0;
}

void SimpleLock::WaitForRelease()
{
	//Wait until we are able to grab a lock, and then release it again
	Acquire();
	Release();
}

void SimpleLock::Release()
{
   uint32_t thread_id = GetThreadId();

   if(_lockCount > 0 && _holderThreadID == thread_id)
   {
      _lockCount--;
      if(_lockCount == 0)
      {
         _holderThreadID = GetThreadIdDefault();
         _lock.clear();
      }
   }
   else
      assert(false);
}


LockHandler::LockHandler(SimpleLock *lock)
{
	_lock = lock;
	_lock->Acquire();
}

LockHandler::~LockHandler()
{
	_lock->Release();
}
