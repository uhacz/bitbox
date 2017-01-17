#include "memory.h"
#include "dlmalloc.h"
#include "debug.h"

struct bxAllocator_Default: public bxAllocator
{
    size_t _allocatedSize;

    bxAllocator_Default()
        : _allocatedSize( 0 )
    {}

    virtual ~bxAllocator_Default() 
    {
        if( _allocatedSize != 0 )
        {
            dlmalloc_stats();
            bxLogError( "Detected memory leaks! Leak size: %u", _allocatedSize );
        }
    }
    virtual void* alloc( size_t size, size_t align )
    {
        void* pointer = dlmemalign( align, size );
        size_t usable_size = dlmalloc_usable_size( pointer );
        _allocatedSize += usable_size;
        return pointer;
    }
    virtual void  free( void* ptr )
    {
        _allocatedSize -= dlmalloc_usable_size( ptr );
        dlfree( ptr );
    }

    virtual size_t allocatedSize() const
    {
        return _allocatedSize;
    }
};

static struct bxAllocator_Default __defaultAllocator;

struct bxAllocator* bxDefaultAllocator()
{
    return &__defaultAllocator;
}