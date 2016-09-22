#include <string.h>

#include "ring_buffer.h"
#include "memory.h"
#include "common.h"


namespace bx{

RingBuffer::RingBuffer( u8* buffer, const u32 buffer_size )
	: _buffer(buffer)
	, _buffer_size(buffer_size)
	, _data_size(0)
	, _head(0)
	, _tail(0)
	, _allocated(0)
{}

void RingBuffer::allocate( size_t size, u32 alignment )
{
	_buffer = (u8*)BX_MALLOC( bxDefaultAllocator(), (u32)size, alignment ); //memory_alloc_aligned( size, alignment );
	_buffer_size = (u32)size;
	_data_size = 0;
	_head = 0;
	_tail = 0;
	_allocated = 1;
}

void RingBuffer::deallocate()
{
	if( !_allocated )
		return;

    BX_FREE( bxDefaultAllocator(), _buffer );
    *this = {};
}

u32 RingBuffer::set( const u8* data, const u32 len )
{
	i32 amount = minOfPair( len, free_space() );
	i32 space = _buffer_size - _head;
	if( space < amount )
	{
		const i32 spaceFromBegin = amount - space;
		
		memcpy( _buffer + _head, data, space );
		_head = ( _head + space ) % _buffer_size;
		
		memcpy( _buffer + _head, data + space, spaceFromBegin );
		_head = ( _head + spaceFromBegin ) % _buffer_size;
	}
	else
	{
		memcpy( _buffer + _head, data, amount );
		_head = ( _head + amount ) % _buffer_size;
	}
	
	_data_size += amount;
	
	return amount;
}

u32 RingBuffer::get( u8* outData, const u32 len )
{
    i32 amount = minOfPair( _data_size, len );
	
	const i32 space = _buffer_size - _tail;
	if( space < amount )
	{
		const i32 spaceFromBegin = amount - space;
		
		memcpy( outData, _buffer + _tail, space );
		_tail = ( _tail + space ) % _buffer_size;
		
		memcpy( outData + space, _buffer + _tail, spaceFromBegin );
		_tail = ( _tail + spaceFromBegin ) % _buffer_size;
	}
	else
	{
		memcpy( outData, _buffer + _tail, amount );
		_tail = ( _tail + amount ) % _buffer_size;
	}
	
	_data_size -= amount;
	return amount;
}

u32 RingBuffer::consume( const u32 len )
{
    i32 amount = minOfPair( _data_size, len );

	const i32 space = _buffer_size - _tail;
	if( space < amount )
	{
		const i32 spaceFromBegin = amount - space;
		_tail = ( _tail + space ) % _buffer_size;
		_tail = ( _tail + spaceFromBegin ) % _buffer_size;
	}
	else
	{
		_tail = ( _tail + amount ) % _buffer_size;
	}
	
	_data_size -= amount;
	return amount;
}

u32 RingBuffer::peek( u8* outData, const u32 len )
{
    i32 amount = minOfPair( _data_size, len );
	const i32 space = _buffer_size - _tail;
	if( space < amount )
	{
		const i32 spaceFromBegin = amount - space;

		memcpy( outData, _buffer + _tail, space );

		const u32 offset = ( _tail + space ) % _buffer_size;
		memcpy( outData + space, _buffer + offset, spaceFromBegin );
	}
	else
	{
		memcpy( outData, _buffer + _tail, amount );
	}

	return amount;
}

void RingBuffer::rewind( const u32 len )
{
    i32 amount = minOfPair( _data_size, len );
	i32 newHead = _head - amount;
	i32 newTail = _tail - amount;
	
	if( newHead < 0 )
		newHead = _buffer_size + newHead;
	
	if( newTail < 0 )
		newTail = _buffer_size + newTail;
		
	_head = newHead;
	_tail = newTail;
}

void RingBuffer::forward( const u32 len )
{
	i32 amount = minOfPair( _data_size, len );
	
	_head = ( _head + amount ) % _buffer_size;
	_tail = ( _tail + amount ) % _buffer_size;
}

void RingBuffer::clear()
{
	_data_size = 0;
	_head = 0;
	_tail = 0;
}

}// 
