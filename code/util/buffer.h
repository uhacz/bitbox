#pragma once

#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct buffer_t
{
    u32 sizeInBytes;
    u32 capacityInBytes;
    u8* data;
};

int  buffer_push      ( struct buffer_t** buff, const void* element, int elementSize, struct bxAllocator* alloc, int alignment );
void buffer_pop       ( struct buffer_t* buff , int elementSize, struct bxAllocator* alloc );
void buffer_removeSwap( struct buffer_t* buff , int offset, int size );
void buffer_remove    ( struct buffer_t* buff , int offset, int size );

//struct buffer_t* buffer_new( u32 capacity, struct bxAllocator* alloc );
void buffer_delete( struct buffer_t** buff, struct bxAllocator* alloc );

#define buffer_pushBack( type, buff, element, alloc ) buffer_push( &buff, &element, sizeof(type), alloc, ALIGNOF(type) )
#define buffer_popBack ( type, buff )                 buffer_pop( buff, sizeof(type) )
#define buffer_clear    ( buff )   ( buff ? buff->sizeInBytes = 0 : (void)buff )
#define buffer_empty    ( buff )   ( buff ? buff->sizeInBytes == 0 : 0 )
#define buffer_size     ( buff )   ( buff ? buff->sizeInBytes : 0 )
#define buffer_capacity ( buff )   ( buff ? buff->capacityInBytes : 0)
#define buffer_count( type, buff ) ( buff ? buff->sizeInBytes / sizeof(type) : 0 )


#ifdef __cplusplus
}
#endif