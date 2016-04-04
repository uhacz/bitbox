#pragma once

#include "array.h"

namespace queue_internal
{
    // Can only be used to increase the capacity.
    template< typename T > void _IncreaseCapacity( queue_t<T>& q, u32 newCapacity)
    {
        u32 end = q.size;
        array_internal::_Grow( q.data, newCapacity );
        q.data.size = newCapacity;
        if ( q.offset +  q.size > end) 
        {
            u32 end_items = end - q.offset;
            memmove( array::begin( q.data ) + newCapacity - end_items, array::begin( q.data ) + q.offset, end_items * sizeof(T) );
            q.offset += newCapacity - end;
        }
    }

    template< typename T > void grow( queue_t<T>& q, u32 minCapacity = 0)
    {
        uint32_t newCapacity = q.size * 2 + 8;
        if (newCapacity < minCapacity)
            newCapacity = minCapacity;

        _IncreaseCapacity( q, newCapacity );
    }
}///

template<typename T > queue_t<T>::queue_t(bxAllocator* alloc)
    : data( alloc )
    , size(0)
    , offset(0)
{}
template< typename T > queue_t<T>::~queue_t()
{}

namespace queue
{
    template< typename T > const T&  front( const queue_t<T>& q ) { return q[0]; }
    template< typename T > T&		 front( queue_t<T>& q )       { return q[0]; }
    template< typename T > const T&  back ( const queue_t<T>& q ) { return q[q.size - 1]; }
    template< typename T > T&		 back ( queue_t<T>& q )       { return q[q.size - 1]; }

    template< typename T > void      clear( queue_t<T>& q ) { q.size = 0; }
    template< typename T > bool      empty( const queue_t<T>& q ) { return q.size == 0; }
    template< typename T > u32       size ( const queue_t<T>& q ) { return q.size; }

    /// Returns the ammount of free space in the queue/ring buffer.
    /// This is the number of items we can push before the queue needs to grow.
    template< typename T > u32      space ( const queue_t<T>& q ) { return q.data.size - q.size; }
    
    template< typename T > void reserve( queue_t<T>& q, u32 size )
    {
        if ( size > q.size )
        {
            queue_internal::_IncreaseCapacity( q, size );
        }
    }

    template< typename T > void push_back( queue_t<T>& q, const T& item )
    {
        if ( !space(q) )
            queue_internal::grow( q );

        q[q.size++] = item;
    }

    template< typename T > void pop_back( queue_t<T>& q )
    {
        --q.size;
    }
    template< typename T > void push_front( queue_t<T>& q, const T &item )
    {
        if ( !space(q) )
            queue_internal::grow(q);

        q.offset = (q.offset - 1 + q.data.size) % q.data.size;
        ++q.size;
        q[0] = item;
    }

    template< typename T > void pop_front( queue_t<T>& q )
    {
        q.offset = (q.offset + 1) % q.data.size;
        --q.size;
    }

    /// Consumes n items from the front of the queue.
    template< typename T > void consume( queue_t<T>& q, u32 n )
    {
        q.offset = (q.offset + n) % q.data.size;
        q.size -= n;
    }
    
    /// Pushes n items to the back of the queue.
    template< typename T > void push( queue_t<T>& q, const T *items, u32 n )
    {
        if ( space(q) < n )
            grow( q, size(q) + n );

        const u32 size = q.data.size;
        const u32 insert = (q.offset + q.size) % size;
        u32 to_insert = n;

        if ( insert + to_insert > size )
            to_insert = size - insert;

        memcpy( array::begin( q.data ) + insert, items, to_insert * sizeof( T ) );

        q.size += to_insert;
        items += to_insert;
        n -= to_insert;

        memcpy( array::begin( q.data ), items, n * sizeof( T ) );

        q.size += n;
    }
}///
