#pragma once
#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*malloc_f )( struct allocator_t* alloc, size_t size, size_t align );
typedef void (*free_f)( struct allocator_t* alloc, void* ptr );

struct allocator_t
{
    malloc_f _malloc;
    free_f _free;

    size_t _allocatedSize;
};


extern void bxMemoryStartUp();
extern void bxMemoryShutDown();

extern struct allocator_t* bxDefaultAllocator();

#define BX_MALLOC( alloc, siz, algn ) (*alloc->_malloc)( alloc, siz, algn )
#define BX_ALLOCATE( alloc, typ ) (typ*)( (*alloc->_malloc)( alloc, sizeof(typ), ALIGNOF(typ)) )
#define BX_FREE( alloc, ptr ) ((*alloc->_free)( alloc, ptr ) )
#define BX_FREE0( alloc, ptr ) { BX_FREE(alloc, ptr); ptr = 0; }

#ifdef __cplusplus
}
#endif