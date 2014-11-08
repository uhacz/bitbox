#include "spin_lock.h"
#include "atomic.h"
#include "../debug.h"
#include "thread.h"

bxSpinLock::bxSpinLock( void )
	: _counter(0)
{}

bxSpinLock::~bxSpinLock( void )
{}

void bxSpinLock::lock()
{
 	while( bxAtomic::compareExchangeAcquire( &_counter, 0, 1 ) != 0 )
 	{
 		bxThread::yeld();
 	}
}

void bxSpinLock::unlock()
{
	SYS_ASSERT( _counter != 0 );
	bxAtomic::exchangeRelease( &_counter, 0 );
}

bool bxSpinLock::try_lock()
{
	return( bxAtomic::compareExchangeAcquire( &_counter, 0, 1 ) == 0 );
	
}
