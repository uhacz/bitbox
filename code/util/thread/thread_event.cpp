#include "thread_event.h"
#include "../debug.h"
#include <new>

bxThreadEvent::bxThreadEvent( bool initial_state, bool manual_reset )
	:_handle(0)
{
	_handle = ::CreateEvent(0, manual_reset, initial_state, 0);
	SYS_ASSERT( _handle != 0 );
}

bxThreadEvent::~bxThreadEvent(void)
{
	if (_handle && _handle != INVALID_HANDLE_VALUE)
	{
		const bool ok = (::CloseHandle(_handle) != 0);
		SYS_ASSERT(ok);
	}
	_handle = 0;
}

void bxThreadEvent::recreate( bool initial_state, bool manual_reset )
{
	this->~bxThreadEvent();
	::new ( this )bxThreadEvent( initial_state, manual_reset );
}

void bxThreadEvent::signal()
{
	SYS_ASSERT(_handle != 0);
	const bool ok = ::SetEvent(_handle) != 0;
    int lastErr = ::GetLastError();
	SYS_ASSERT(ok);
}

void bxThreadEvent::reset()
{
	SYS_ASSERT(_handle != 0);
	const bool ok = ::ResetEvent(_handle) != 0;
	SYS_ASSERT(ok);
}

void bxThreadEvent::waitInfinite() const
{
	SYS_ASSERT(_handle != 0);
	const DWORD result = ::WaitForSingleObject(_handle, INFINITE);
	SYS_ASSERT(result == WAIT_OBJECT_0);
}

bool bxThreadEvent::waitTimeout( long milliseconds ) const
{
	SYS_ASSERT(_handle != 0);
	const DWORD result = WaitForSingleObject(_handle, milliseconds);
	SYS_ASSERT(result == WAIT_TIMEOUT || result == WAIT_OBJECT_0);
	return result == WAIT_OBJECT_0;
}
