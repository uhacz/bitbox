#pragma once

#include "type.h"
#include "debug.h"


struct bxRangeSplitter
{
private:
    bxRangeSplitter( u32 ne, u32 gs ): nElements( ne ), grabSize(gs), grabbedElements(0) {}
public:

    static inline bxRangeSplitter splitByTask( u32 nElements, u32 nTasks )   { return bxRangeSplitter( nElements, 1 + ( (nElements-1) / nTasks ) ); }
    static inline bxRangeSplitter splitByGrab( u32 nElements, u32 grabSize ) { return bxRangeSplitter( nElements, grabSize ); }

    inline u32 elements_left() const { return nElements - grabbedElements; }
    inline u32 next_grab()
    {
        const u32 eleft = elements_left();
        const u32 grab = ( eleft < grabSize ) ? eleft : grabSize;
        grabbedElements += grab;
        SYS_ASSERT( grabbedElements <= nElements );
        return grab;
    }

    const u32 nElements;
    const u32 grabSize;
    u32 grabbedElements;
};
