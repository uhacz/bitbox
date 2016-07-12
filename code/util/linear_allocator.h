#pragma once

namespace bx
{
    struct LinearAllocator
    {
        explicit LinearAllocator( size_t size );
        LinearAllocator( void* start, void* end );

        void* alloc( size_t size, unsigned alignment, unsigned offset = 0 );
        void free();

    private:
        char* _current = nullptr;
        char* _start = nullptr;
        char* _end = nullptr;
    };
}////