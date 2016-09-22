#pragma once
#include "type.h"

namespace bx{
class RingBuffer
{
public:
    RingBuffer() {}
	RingBuffer( u8* buffer, const u32 buffer_size );

	void allocate( size_t size, u32 alignment = sizeof(void*) );
	void deallocate();

	// Try to add data to the buffer.
	// @return the amount of bytes actually buffered.
	u32 set( const u8* data, const u32 len );
	template< class T >
	u32 push( const T* data )	{ return set( (const u8*)data, sizeof(T) ); }
		
	// Request 'len' bytes from the buffer.
	// @return the amount of bytes actually copied.
	u32 get( u8* outData, const u32 len );
	template < class T > 
	u32 pop( T* data )	{ return get( (u8*)data, sizeof(T) ); }

	// consume data without copy
	// @return the amount of bytes actually consumed.
	u32 consume( const u32 len );

	// Peek 'len' bytes from the buffer. This function does not
	// affect amout of data in buffer
	// @return the amount of bytes actually copied.
	u32 peek( u8* outData, const u32 len );

	// The amount of data the buffer can currently receive on one set() call.
	u32 free_space()  const { return _buffer_size - _data_size; }

	// The size of buffer in bytes
	u32 buffer_size()	const { return _buffer_size; }
	
	// The size of bytes in buffer
	u32 data_size()		const { return _data_size; }
	
	// current data offset in bytes
	u32 head_offset()		const	{ return _head; }
	u32 tail_offset()		const	{ return _tail; }
	
	// move head and tail without touching data
	void rewind( const u32 len );
	void forward( const u32 len );
	
	void clear();

private:
	u8* _buffer = nullptr;
	u32 _buffer_size = 0;
    u32 _data_size = 0;
    u32 _head = 0;
    u32 _tail = 0;
    u32 _allocated = 0;
};



template< class T >
class RingArray
{
public:
	void init( T* arr, u32 capacity )
	{
		_array = arr;
		_capacity = capacity;
		_size = 0;
		_head = 0;
		_tail = 0;
	}

	bool push_back( const T& in = T() )
	{
		if( !free_space() ) return false;

		_array[_head] = in;
		_head = ( _head + 1 ) % _capacity;
		++_size;

		return true;
	}

    void push_back_ring( const T& in = T() )
    {
        _array[_head] = in;
		_head = ( _head + 1 ) % _capacity;
        _size = ( _size >= _capacity ) ? _size : _size + 1;
    }

	bool pop_front( T* out )
	{
		if( empty() ) return false;

		*out = _array[_tail];
		_tail = ( _tail + 1 ) % _capacity;
		--_size;

		return true;
	}

    void replace_head( const T& t )
    {
        _array[_head] = t;
    }

	u32 size()		const { return _size; }
	u32 capacity()	const { return _capacity; }
	u32 free_space()const { return _capacity - _size; }
	bool empty()	const { return _size == 0; }

    u32 head() const { return _head; }
    u32 tail() const { return _tail; }

private:
	T* _array = nullptr;
	u32 _size = 0;
    u32 _capacity = 0;
    u32 _head = 0;
    u32 _tail = 0;
};

}//
