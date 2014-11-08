#include "semaphore.h"
#include "..\debug.h"

bxSemaphore::bxSemaphore()
    : handle(0)
{}

bxSemaphore::bxSemaphore( u32 initial_count, u32 max_count )
	: handle(0)
{
	create( initial_count, max_count );
}

bxSemaphore::~bxSemaphore()
{
	destroy();
}

void bxSemaphore::create( u32 initial_count, u32 max_count )
{
	handle = CreateSemaphore( NULL, initial_count, max_count, NULL );
}

void bxSemaphore::destroy()
{
    CloseHandle( handle );
    handle = 0;
}


long bxSemaphore::signal( u32 count ) const 
{
	long prev_count = 0;
	BOOL res = ReleaseSemaphore( handle, count, &prev_count );
	int err = GetLastError();
	return prev_count;
}

void bxSemaphore::wait( u32 time_ms ) const
{
	WaitForSingleObject( handle, time_ms );
}

void bxSemaphore::wait_infinite() const
{
	WaitForSingleObject( handle, INFINITE );
}
