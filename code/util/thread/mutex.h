#pragma once

#include "atomic.h"

template< class T>
class bxScopeLock
{
public:
	bxScopeLock( T& l )
		: _lock(l)
	{
		_lock.lock();
	}

	~bxScopeLock()
	{
		_lock.unlock();
	}

private:
	const bxScopeLock& operator = ( const bxScopeLock& other ) { (void)&other; return *this; }

private:
	T& _lock;
};

struct bxMutex
{
	bxMutex( u32 spin_count = 0 );
	~bxMutex(void);

	void setSpinCount( int spin );

	void lock();
	void unlock();
	bool tryLock();

private:
	bxMutexHandle _handle;
};



class bxBenaphore
{
private:
	atomic32 _counter;
	bxSemaphoreHandle _semaphore;

public:
	bxBenaphore();

	~bxBenaphore();

	void lock();
	void unlock();
};

class bxRecursiveBenaphore
{
public:
    bxRecursiveBenaphore();
    ~bxRecursiveBenaphore();

    void lock();
	void unlock();
private:
    u64 _owner;
    u32 _recursion;
    atomic32 _counter;
    bxSemaphoreHandle _semaphore;
};

typedef bxScopeLock<bxMutex>                bxScopeMutex;
typedef bxScopeLock<bxBenaphore>            bxScopeBenaphore;
typedef bxScopeLock<bxRecursiveBenaphore>   bxScopeRecursiveBenaphore;

