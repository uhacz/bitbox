#pragma once

#include "debug.h"

struct bxBufferChunker
{
    bxBufferChunker( void* mem, int mem_size )
        : current( (unsigned char*)mem ), end( (unsigned char*)mem + mem_size )
    {}

    template< typename T >
    T* add( int count = 1 )
    {
        unsigned char* result = current;

        unsigned char* next = current + count * sizeof(T);
        current = next;
        return (T*)result;
    }

    void check()
    {
        SYS_ASSERT( current == end );
    }

    unsigned char* current;
    unsigned char* end;
};