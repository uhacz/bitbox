#pragma once
#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*bxMallocFunc )( struct bxAllocator* alloc, size_t size, size_t align );
typedef void (*bxFreeFunc )( struct bxAllocator* alloc, void* ptr );

struct bxAllocator
{
    bxMallocFunc _malloc;
    bxFreeFunc _free;

    size_t _allocatedSize;
};


extern void bxMemoryStartUp();
extern void bxMemoryShutDown();

extern struct bxAllocator* bxDefaultAllocator();

#define BX_MALLOC   ( alloc, size, align ) ( (*alloc->_malloc)( alloc, size, align ) )
#define BX_ALLOCATE ( alloc, typ ) (typ*)( (*alloc->_malloc)( alloc, sizeof(typ), ALIGNOF(typ)) )
#define BX_FREE     ( alloc, ptr ) ((*alloc->_free)( alloc, ptr ) )
#define BX_FREE0    ( alloc, ptr ) { BX_FREE(alloc, ptr); ptr = 0; }

#ifdef __cplusplus
}
#endif