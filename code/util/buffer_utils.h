#pragma once

#include "debug.h"

struct bxBufferChunker
{
    bxBufferChunker( void* mem, int mem_size )
        : current( (unsigned char*)mem ), end( (unsigned char*)mem + mem_size )
    {}

    template< typename T >
    T* add( int count = 1, int alignment = 4 )
    {
        unsigned char* result = (unsigned char*)TYPE_ALIGN( current, alignment );

        unsigned char* next = result + count * sizeof( T );
        current = next;
        return (T*)result;
    }
    unsigned char* addBlock( int size, int alignment = 4 )
    {
        unsigned char* result = (unsigned char*)TYPE_ALIGN( current, alignment );

        unsigned char* next = result + size;
        current = next;
        return result;
    }

    void check()
    {
        SYS_ASSERT( current == end );
    }

    unsigned char* current;
    unsigned char* end;
};