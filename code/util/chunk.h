#pragma once

#include "range_splitter.h"
#include "debug.h"

struct bxChunk
{
    i32 begin = 0;
    i32 end = 0;
    i32 current = 0;
};

inline void bxChunk_create( bxChunk* chunks, int nChunks, int nItems )
{
    const int N_TASKS = nChunks;
    bxRangeSplitter splitter = bxRangeSplitter::splitByTask( nItems, N_TASKS );
    int ilist = 0;
    while ( splitter.elementsLeft() )
    {
        const int begin = splitter.grabbedElements;
        const int grab = splitter.nextGrab();
        const int end = begin + grab;

        SYS_ASSERT( ilist < N_TASKS );
        bxChunk& c = chunks[ilist++];
        c.begin = begin;
        c.end = end;
        c.current = begin;
    }

    SYS_ASSERT( ilist <= N_TASKS );
}


