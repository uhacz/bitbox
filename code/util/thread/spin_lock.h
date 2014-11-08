#pragma once

#include "../type.h"

class bxSpinLock
{
public:
	bxSpinLock(void);
	~bxSpinLock(void);

	void lock();
	void unlock();
	bool try_lock();

private:
	atomic32 _counter;
};
