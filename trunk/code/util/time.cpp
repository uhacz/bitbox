#include "time.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace bxTime
{
	
	static inline i64 herz()
	{
		LARGE_INTEGER tf;
		QueryPerformanceFrequency(&tf);
		return tf.QuadPart;
	}

	u64 ms()
	{
		static u64 clockPerSec = herz();
		LARGE_INTEGER ct;
		QueryPerformanceCounter( &ct );

		return unsigned( ct.QuadPart / ( clockPerSec / 1000 ) );
	}
	u64 us()
	{
		static u64 clockPerSec = herz();
		static double clockPerUSrcp = 1000000.0 / clockPerSec;
		LARGE_INTEGER ct;
		QueryPerformanceCounter( &ct );

		return (u64)(ct.QuadPart * clockPerUSrcp);
	}

    void sleep( int milis )
    {
        Sleep( milis );
    }
}//

bxTimeQuery bxTimeQuery::begin()
{
	bxTimeQuery tq;
    LARGE_INTEGER ct;
	QueryPerformanceCounter( &ct );
	tq.tickStart = ct.QuadPart;
    return tq;
}
void bxTimeQuery::end( bxTimeQuery* tq )
{
	LARGE_INTEGER ct;
	BOOL bres = QueryPerformanceCounter( &ct );
	tq->tickStop = ct.QuadPart;

	LARGE_INTEGER tf;
	bres = QueryPerformanceFrequency(&tf);
                                                                       
    const u64 numTicks = ( (u64)( tq->tickStop - tq->tickStart ) );
    const double durationUS = double( numTicks * 1000000 ) / (double)tf.QuadPart;
	tq->durationUS = (u64)durationUS;
}
