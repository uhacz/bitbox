#include "thread.h"
#include "../debug.h"
#include "../memory.h"
#include "atomic.h"
#include <process.h>

bxThreadHandle bxThread::startThread( bxThreadRun* run, const char* name )
{
	SYS_ASSERT( run != 0 );

	bxThreadHandle handle;
	handle._run = run;
	handle._sysHandle = _beginthreadex( NULL, 0, _ThreadRun, (void*)run, 0, &handle._sysId );
    SetThreadPriority( (HANDLE)handle._sysHandle, THREAD_PRIORITY_HIGHEST );
	if( !handle._sysHandle )
	{
		bxLogError( "Couldn't start thread" );
		//MAKE_DELETE( default_allocator(), ThreadRun, run );
        //memory_delete( run );
		handle._run = 0;
	}

	if( name )
	{
		setThreadName( handle._sysId, name );
	}

	return handle;
}

bxThreadHandle bxThread::currentThread()
{
	bxThreadHandle handle;
	handle._run = 0;
	handle._sysHandle = (uptr)GetCurrentThread();
	handle._sysId = GetCurrentThreadId();

	return handle;
}

void bxThread::stopThread( bxThreadHandle* thread_handle, bool delete_run_object )
{
	const DWORD res = ::WaitForSingleObject( (HANDLE)thread_handle->_sysHandle, INFINITE );
	SYS_ASSERT(res == WAIT_OBJECT_0);

	const BOOL ok = ::CloseHandle( (HANDLE)thread_handle->_sysHandle );
	SYS_ASSERT( ok == TRUE );
	thread_handle->_sysHandle = 0;
    thread_handle->_sysId = 0;
	if( delete_run_object )
    {
        BX_DELETE( bxDefaultAllocator(), thread_handle->_run );
        thread_handle->_run = 0;
    }
		//memory_delete( thread_handle->_run );
}

u32 __stdcall bxThread::_ThreadRun( void* arg )
{
	bxThreadRun* run = (bxThreadRun*)arg;
	return run->run();
}

void bxThread::sleep( long ms )
{
	Sleep( ms );
}

void bxThread::yeld()
{
//	::SwitchToThread();
	::Sleep(0);
}

i32 bxThread::currentThreadId()
{
	return ::GetCurrentThreadId();
}

u32 bxThread::numberOfProcessors()
{
	SYSTEM_INFO sys_info;
	GetSystemInfo( &sys_info );

	return sys_info.dwNumberOfProcessors;
}

__declspec(thread) const char*		s_currentThreadName(0);

// See http://www.codeproject.com/KB/threads/Name_threads_in_debugger.aspx
// and/or http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // must be 0x1000
	LPCSTR szName; // pointer to name (in user addr space)
	DWORD dwThreadID; // thread ID (-1=caller thread)
	DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

void bxThread::setThreadName( u32 thread_id, const char* name )
{
	s_currentThreadName = name;

	// No need to set thread name, if there's no debugger attached.
// 	if (!IsDebuggerPresent())
// 		return;

	THREADNAME_INFO info;
	{
		info.dwType = 0x1000;
		info.szName = name;
		info.dwThreadID = (DWORD)thread_id;
		info.dwFlags = 0;
	}
	__try
	{
#ifdef x64
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info);
#else
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info);		
#endif
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}				
}
