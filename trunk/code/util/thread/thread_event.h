#pragma once

#include "atomic.h"

class bxThreadEvent
{
public:
	bxThreadEvent( bool initial_state = false, bool manual_reset = true );
	~bxThreadEvent(void);

	void recreate( bool initial_state, bool manual_reset );

	void signal();
	void reset();
	void waitInfinite() const;
	bool waitTimeout( long milliseconds ) const;

private:
	bxThreadEventHandle _handle;
};
