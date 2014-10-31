#include "buffer.h"
#include "memory.h"
#include "debug.h"
#include <string.h>

static struct buffer_t* _Buffer_allocate( int newCapacityInBytes, struct allocator_t* alloc, int alignment )
{
    struct buffer_t* b = 0;
    int memorySize = 0;
    void* memory = 0;

    memorySize += sizeof( struct buffer_t );
    memorySize = TYPE_ALIGN( memorySize, alignment );
    memorySize += newCapacityInBytes;

    memory = BX_MALLOC( alloc, memorySize, alignment );
    memset( memory, 0, memorySize );

    b = (struct buffer_t*)memory;
    b->sizeInBytes = 0;
    b->capacityInBytes = newCapacityInBytes;
    b->data = (u8*)TYPE_ALIGN( ( b + 1 ), alignment );

    return b;
}

static void _Buffer_grow( struct buffer_t** buff, int newCapacityInBytes, struct allocator_t* alloc, int alignment )
{
    struct buffer_t* oldBuff = buff[0];
    struct buffer_t* newBuff = 0;

    if( !oldBuff )
    {
        buff[0] = _Buffer_allocate( newCapacityInBytes, alloc, alignment );
    }
    else if( newCapacityInBytes > (int)oldBuff->capacityInBytes )
    {
        newBuff = _Buffer_allocate( newCapacityInBytes, alloc, alignment );
        newBuff->sizeInBytes = oldBuff->sizeInBytes;
        memcpy( newBuff->data, oldBuff->data, newBuff->sizeInBytes );

        buff[0] = newBuff;

        BX_FREE0( alloc, oldBuff );
    }
}


int buffer_push( struct buffer_t** buff, const void* element, int elementSize, struct allocator_t* alloc, int alignment )
{
    int offset, needToGrow, newCapacity;
    
    needToGrow = ( !buff[0] ) ? 1 : buff[0]->sizeInBytes + elementSize > buff[0]->capacityInBytes;
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

void buffer_pop( struct buffer_t* buff , int elementSize, struct allocator_t* alloc )
{
    int n = 0;

    if( !buff )
        return;

    n = ( (u32)elementSize > buff->sizeInBytes ) ? buff->sizeInBytes : elementSize;
    buff->sizeInBytes -= n;
}

void buffer_removeSwap( struct buffer_t* buff, int offset, int size )
{
    SYS_ASSERT( size != 0 );
	SYS_ASSERT( (u32)( offset + size ) <= buff->sizeInBytes );
	
	if( size == buff->sizeInBytes )
	{
        buff->sizeInBytes = 0;
	}
	else
	{
		u8* toErase = buff->data + offset;
		u8* toCopy = buff->data + buff->sizeInBytes - size;
		memcpy( toErase, toCopy, size );
		buff->sizeInBytes -= size;
	}
	
#ifdef _DEBUG
	memset( buff->data + buff->sizeInBytes, 0xBU, size );
#endif
}

void buffer_remove( struct buffer_t* buff, int offset, int size )
{
    SYS_ASSERT( size != 0 );
	SYS_ASSERT( (u32)( offset + size ) <= buff->sizeInBytes );

	if( size == buff->sizeInBytes )
	{
		buff->sizeInBytes = 0;
	}
	else
	{
		u8* to_erase = buff->data + offset;
		u8* to_copy = buff->data + offset + size;
		const u32 to_copy_size = buff->sizeInBytes - (offset+size);
		memcpy( to_erase, to_copy, to_copy_size );
		buff->sizeInBytes -= size;
	}

#ifdef _DEBUG
	memset( buff->data + buff->sizeInBytes, 0xBU, size );
#endif
}

void buffer_delete( struct buffer_t** buff, struct allocator_t* alloc )
{
    BX_FREE0( alloc, buff[0] );
}
