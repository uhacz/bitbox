#pragma once

#include "atomic.h"

struct bxSemaphore
{
	bxSemaphore();
    bxSemaphore( u32 initial_count, u32 max_count );
	~bxSemaphore();

	void create( u32 initial_count, u32 max_count );
    void destroy();

	long signal( u32 count ) const;
	void wait_infinite()	 const;
	void wait( u32 time_ms ) const;

private:
	bxSemaphoreHandle handle;
};
