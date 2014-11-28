#pragma once

#include "type.h"

namespace bxTime
{
	u64 ms();
	u64 us();

    void sleep( int milis );

    inline double toSeconds( const u64 micros )
    {
        return double( micros ) * 0.000001;
    }
}//

struct bxTimeQuery
{
	bxTimeQuery()
		: tickStart(0)
		, tickStop(0)
		, durationUS(0)
	{}

	u64 tickStart;
	u64 tickStop;
	u64 durationUS;

    static bxTimeQuery begin();
    static void end( bxTimeQuery* tq );

    inline u64 durationMS() const
    {
        return durationUS / 1000;
    }

    inline double durationS()
    {
        return bxTime::toSeconds( durationUS );
    }
};
