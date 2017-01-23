#pragma once

#include <util/time.h>

namespace bx
{

struct GameTime
{
    u64 time_us = 0;
    u64 time_not_paused_us = 0;
    u64 delta_time_us = 0;

    bool  IsPaused() const { return time_us > 0 && delta_time_us == 0; }
    float DeltaTimeSec() const { return (float)bxTime::toSeconds( delta_time_us ); }
    float TimeSec() const { return (float)bxTime::toSeconds( time_us ); }
};

}//