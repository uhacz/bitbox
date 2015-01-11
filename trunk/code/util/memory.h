#pragma once
#include "type.h"

struct bxAllocator
{
    virtual ~bxAllocator() {}
    virtual void* alloc( size_t size, size_t align ) = 0;
    virtual void  free( void* ptr ) = 0;
    virtual size_t allocatedSize() const = 0;
};

extern bxAllocator* bxDefaultAllocator();

#define BX_MALLOC( a, siz, algn ) a->alloc( siz, algn )
#define BX_ALLOCATE( a, typ ) (typ*)( a->alloc( sizeof(typ), ALIGNOF(typ)) )
#define BX_FREE( a, ptr ) (a->free( ptr ) )
#define BX_FREE0( a, ptr ) { BX_FREE(a, ptr); ptr = 0; }

#include <new>
#define BX_NEW(a, T, ...) (new ((a)->alloc(sizeof(T), ALIGNOF(T))) T(__VA_ARGS__))
#define BX_NEW1( T, ... ) BX_NEW( bxDefaultAllocator(), T, __VA_ARGS__ )
template<typename T>
void BX_DELETE( bxAllocator* alloc, T* ptr )
{
    if( ptr )
    {
        ptr->~T();
        alloc->free( ptr );
    }
}
#define BX_DELETE0( a, ptr ) { BX_DELETE( a, ptr ); ptr = 0; }
#define BX_DELETE01( ptr ) { BX_DELETE( bxDefaultAllocator(), ptr ); ptr = 0; }