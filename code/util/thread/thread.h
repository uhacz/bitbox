#pragma once

#include <util/type.h>
#include <intrin.h>

/// thread interface
class bxThreadRun
{
public:
    virtual ~bxThreadRun() {}
	virtual u32 run() = 0;
};

/// handle 
class bxThreadHandle
{
public:
    bxThreadHandle()
        : _sysHandle(0)
        , _sysId(0)
        , _run(0)
    {}

	bool ok() const { return _sysHandle != 0; }
	bool equal( const bxThreadHandle& h ) const { return _sysId == h._sysId; }

private:
	uptr _sysHandle;
    u32 _sysId;
	bxThreadRun* _run;

	friend class bxThread;
};

/// thread creation
class bxThread
{
public:
	static bxThreadHandle startThread( bxThreadRun* run, const char* name = 0 );
	static bxThreadHandle currentThread();

	static void stopThread( bxThreadHandle* thread_handle, bool delete_run_object );

	static void sleep( long ms );
	static void yeld();
	static i32 currentThreadId();
	static u32 numberOfProcessors();
	static void setThreadName( u32 thread_id, const char* name );
	__forceinline static void machinePause( long loops );

private:
	static u32 __stdcall _ThreadRun( void* arg );
};

//////////////////////////////////////////////////////////////////////////
 __forceinline void bxThread::machinePause( long loops )
 {
#ifdef x86
	 _asm
 	{
 		mov	eax, loops
 	waitlabel:
 		pause
 		add	eax, -1
 		jne	waitlabel
 	}
#elif x64
	 long i = 0;
	 do 
	 {
		 __nop();
	 } while ( ++i < loops );
#else
#error not implemented
#endif
 }
