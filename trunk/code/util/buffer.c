#include "buffer.h"
#include "memory.h"
#include <string.h>

static int _Buffer_calculateMemorySize( int newCapacityInBytes, int alignment )
{
    int memorySize = 0;
    memorySize += sizeof( struct buffer_t );
    memorySize = TYPE_ALIGN( memorySize, alignment );
    memorySize += newCapacityInBytes;
    
    return memorySize;
}
static struct buffer_t* _Buffer_allocate( int memorySize, struct bxAllocator* alloc, int alignment )
{
    struct buffer_t* b = 0;
    void* memory = BX_MALLOC( alloc, memorySize, alignment );
    memset( memory, 0, memorySize );

    b = (struct buffer_t*)memory;
    b->sizeInBytes = 0;
    b->capacityInBytes = newCapacityInBytes;
    b->data = (u8*)TYPE_ALIGN( ( b + 1 ), alignment );

    return b;
}

static void _Buffer_grow( struct buffer_t** buff, int newCapacityInBytes, struct bxAllocator* alloc, int alignment )
{
    struct buffer_t* oldBuff = buff[0];
    struct buffer_t* newBuff = 0;

    if( !oldBuff )
    {
        int memorySize = _Buffer_calculateMemorySize( newCapacityInBytes, alignment );    
        buff[0] = _Buffer_allocate( memorySize, alloc, alignment );
    }
    else if( newCapacityInBytes > (int)oldBuff->capacityInBytes )
    {
        int memorySize = _Buffer_calculateMemorySize( newCapacityInBytes, alignment );
        newBuff = _Buffer_allocate( memorySize, alloc, alignment );
        
        newBuff->sizeInBytes = oldBuff->sizeInBytes;
        memcpy( newBuff->data, oldBuff->data, newBuff->sizeInBytes );

        buff[0] = newBuff;

        BX_FREE0( alloc, oldBuff );
    }
}


int buffer_push( struct buffer_t** buff, const void* element, int elementSize, struct bxAllocator* alloc, int alignment )
{
    int offset, needToGrow, newCapacity;
    
    needToGrow = !buff[0];
    needToGrow |= buff[0]->sizeInBytes + elementSize < buff[0]->capacityInBytes;
    
    newCapacity = ( !buff[0] ) ? elementSize * 16 : 2*(buff[0]->sizeInBytes + elementSize);

    if( needToGrow )
    {
        _Buffer_grow( buff, newCapacity, alloc, alignment );
    }

    offset = buff[0]->sizeInBytes;
    memcpy( buff[0]->data + offset, element, elementSize );
    buff[0]->sizeInBytes += elementSize;
    return offset;
}

void buffer_pop( struct buffer_t* buff , int elementSize, struct bxAllocator* alloc )
{
    int n = 0;

    if( !buff )
        return;

    n = ( elementSize > buff->sizeInBytes ) ? buff->sizeInBytes : elementSize;
    buff->sizeInBytes -= n;
}

void buffer_removeSwap( struct buffer_t* buff, int offset, int size )
{

}

void buffer_remove( struct buffer_t* buff, int offset, int size )
{

}

//struct buffer_t* buffer_new( u32 capacity, struct bxAllocator* alloc )
//{
//
//}

void buffer_delete( struct buffer_t** buff, struct bxAllocator* alloc )
{

}
