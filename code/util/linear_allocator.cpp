#include "linear_allocator.h"
#include "memory.h"

namespace bx
{
    LinearAllocator::LinearAllocator( size_t size )
    {
        _start = (char*)BX_MALLOC( bxDefaultAllocator(), size, sizeof( void* ) );
        _current = _start;
        _end = _start + size;
        _memory_owner = true;
    }

    LinearAllocator::LinearAllocator( void* start, void* end )
    {
        _start = (char*)start;
        _end = (char*)end;
        _current = _start;
    }

    LinearAllocator::~LinearAllocator()
    {
        if( _memory_owner )
        {
            BX_FREE0( bxDefaultAllocator(), _start );
        }
    }

    void* LinearAllocator::alloc( size_t size, unsigned alignment, unsigned offset /*= 0 */ )
    {
        // offset pointer first, align it, and offset it back
        _current = (char*)memory::alignForward( _current + offset, alignment ) - offset;

        void* userPtr = _current;
        _current += size;

        if( _current >= _end )
        {
            // out of memory
            return nullptr;
        }

        return userPtr;
    }

    void LinearAllocator::free()
    {
        _current = _start;
    }

}////