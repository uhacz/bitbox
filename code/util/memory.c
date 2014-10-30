#include "memory.h"
#include "dlmalloc.h"
#include "stdio.h"

static void* bxDefaultMalloc( struct allocator_t* alloc, size_t size, size_t align )
{
    void* pointer = dlmemalign( align, size );
    alloc->_allocatedSize = dlmalloc_usable_size( pointer );
    return pointer;
}

static void bxDefaultFree( struct allocator_t* alloc, void* ptr )
{
    alloc->_allocatedSize -= dlmalloc_usable_size( ptr );
    dlfree( ptr );
}
static struct allocator_t* __defaultAllocator = 0;

void bxMemoryStartUp()
{
    __defaultAllocator = (struct allocator_t*)dlmemalign( 64, sizeof( struct allocator_t ) );
    __defaultAllocator->_malloc = bxDefaultMalloc;
    __defaultAllocator->_free = bxDefaultFree;
    __defaultAllocator->_allocatedSize = 0;
}
void bxMemoryShutDown()
{
    if( __defaultAllocator->_allocatedSize != 0 )
    {
        dlmalloc_stats();
        printf( "Detected memory leaks! Leak size: %u", __defaultAllocator->_allocatedSize );
    }

    dlfree( __defaultAllocator );
    __defaultAllocator = 0;
}

struct allocator_t* bxDefaultAllocator()
{
    return __defaultAllocator;
}