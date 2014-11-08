#include "mutex.h"
#include "../debug.h"

#ifdef WIN32

bxMutex::bxMutex( u32 spin_count )
{
	InitializeCriticalSectionAndSpinCount( &_handle, spin_count );
}

bxMutex::~bxMutex(void)
{
    DeleteCriticalSection( &_handle );
}

void bxMutex::setSpinCount( int spin )
{
	SetCriticalSectionSpinCount( &_handle, spin );
}

void bxMutex::lock( )
{
	EnterCriticalSection( &_handle );
}

void bxMutex::unlock( )
{
	LeaveCriticalSection( &_handle );
}

bool bxMutex::tryLock( )
{
	// If the critical section is successfully entered or the current thread already owns the critical section, the return value is nonzero.
	// If another thread already owns the critical section, the return value is zero.
	return ( TryEnterCriticalSection( &_handle ) != 0 );
}


bxBenaphore::bxBenaphore()
	:_counter(0)
{
	_counter = 0;
	_semaphore = CreateSemaphore(NULL, 0, 1, NULL);
}

bxBenaphore::~bxBenaphore()
{
	CloseHandle(_semaphore);
}

void bxBenaphore::lock()
{
	if (InterlockedIncrement(&_counter) > 1)
	{
		WaitForSingleObject(_semaphore, INFINITE);
	}
}

void bxBenaphore::unlock()
{
	if (InterlockedDecrement(&_counter) > 0)
	{
		ReleaseSemaphore(_semaphore, 1, NULL);
	}
}

bxRecursiveBenaphore::bxRecursiveBenaphore()
{
    _counter = 0;
    _owner = 0;            // an invalid thread ID
    _recursion = 0;
    _semaphore = CreateSemaphore(NULL, 0, 1, NULL);
}
bxRecursiveBenaphore::~bxRecursiveBenaphore()
{
    CloseHandle( _semaphore );
}

void bxRecursiveBenaphore::lock()
{
    DWORD tid = GetCurrentThreadId();
    if ( InterlockedIncrement( &_counter ) > 1) // x86/64 guarantees acquire semantics
    {
        if (tid != _owner)
        {
            WaitForSingleObject(_semaphore, INFINITE);
        }
    }
    //--- We are now inside the Lock ---
    _owner = tid;
    _recursion++;
}
void bxRecursiveBenaphore::unlock()
{
    uptr tid = GetCurrentThreadId();
    SYS_ASSERT( tid == _owner );
    DWORD recur = --_recursion;
    if (recur == 0)
    {
        _owner = 0;
    }

    DWORD result = InterlockedDecrement(&_counter); // x86/64 guarantees release semantics
    if (result > 0)
    {
        if (recur == 0)
        {
            ReleaseSemaphore(_semaphore, 1, NULL);
        }
    }
    //--- We are now outside the Lock ---
}

#endif
