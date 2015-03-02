#pragma once

namespace queue_internal
{
    // Can only be used to increase the capacity.
    template< typename T > void _IncreaseCapacity( queue_t<T>& q, u32 newCapacity)
    {
        u32 end = q.size
        array_internal::_Grow( q.data, newCapacity );

        if ( q.offset +  q.size > end) 
        {
            u32 end_items = end - _offset;
            memmove( array::begin( q.data ) + newCapacity - end_items, array::begin( q.data ) + _offset, end_items * sizeof(T) );
            _offset += newCapacity - end;
        }
    }

    template< typename T > void grow( queue_t<T>& q, u32 minCapacity = 0)
    {
        uint32_t newCapacity = q.size * 2 + 8;
        if (newCapacity < minCapacity)
            newCapacity = minCapacity;

        _IncreaseCapacity( newCapacity );
    }
}///


