#pragma once

#include "containers.h"
#include <memory.h>

namespace array_internal
{
    template< typename T > void _Grow( array_t<T>& arr, int newCapacity )
    {
        T* newData = 0;
        if( newCapacity > 0 )
        {
            newData = (T*)BX_MALLOC( arr.allocator, sizeof(T)*newCapacity, ALIGNOF( T ) );
            const int toCopy = ( (u32)newCapacity < arr.size ) ? (u32)newCapacity : arr.size;
            memcpy( newData, arr.data, sizeof(T)*toCopy );
        }

        BX_FREE0( arr.allocator, arr.data );
        arr.data = newData;
        arr.capacity = newCapacity;
    }
}///

namespace array
{
    template< typename T > T*       begin   ( array_t<T>& arr ) { return arr.data; }
    template< typename T > T*       end     ( array_t<T>& arr ) { return arr.data + arr.size; }
    template< typename T > const T* begin   ( const array_t<T>& arr ) { return arr.data; }
    template< typename T > const T* end     ( const array_t<T>& arr ) { return arr.data + arr.size; }
    template< typename T > int      capacity( const array_t<T>& arr ) { return arr.capacity; }
    template< typename T > int      size    ( const array_t<T>& arr ) { return arr.size; }
    template< typename T > int      empty   ( const array_t<T>& arr ) { return arr.size == 0; }
    template< typename T > int      any     ( const array_t<T>& arr ) { return arr.size != 0; }
    template< typename T > T&       front   ( array_t<T>& arr )       { return arr.data[0]; }
    template< typename T > const T& front   ( const array_t<T>& arr ) { return arr.data[0]; }
    template< typename T > T&       back    ( array_t<T>& arr )       { return arr.data[arr.size-1]; }
    template< typename T > const T& back    ( const array_t<T>& arr ) { return arr.data[arr.size-1]; }
    template< typename T > void     clear   ( array_t<T>& arr )       { arr.size = 0; }
}///

template< typename T > array_t<T>::array_t( bxAllocator* alloc /* = bxDefaultAllocator */ )
    : size(0)
    , capacity(0)
    , allocator( alloc )
    , data(0)
{}

template< typename T >
array_t<T>::~array_t()
{
    BX_FREE0( allocator, data );
}

namespace array
{
    template< typename T > int push_back( array_t<T>& arr, const T& value )
    {
        if( arr.size + 1 > arr.capacity )
        {
            array_internal::_Grow( arr, arr.capacity * 2 + 8 );
        }

        arr.data[arr.size] = value;
        return (int)( arr.size++ );
    }

    template< typename T > void pop_back( array_t<T>& arr )
    {
        arr.size = ( arr.size > 0 ) ? --arr.size : 0;
    }

    template< typename T > void erase_swap( array_t<T>& arr, int pos )
    {
        const u32 upos = (u32)pos;
        if( upos > arr.size )
            return;

        if( upos != arr.size - 1 )
        {
            arr.data[upos] = back( arr );
        }
        pop_back( arr );
    }

    template< typename T > void erase( array_t<T>& arr, int pos )
    {
        const u32 upos = (u32)pos;
        if( upos > arr.size )
            return;

        for( u32 i = upos + 1; i < arr.size; ++i )
        {
            arr.data[i-1] = arr.data[i];
        }
        pop_back( arr );
    }

    template< typename T > void reserve ( array_t<T>& arr, int newCapacity )
    {
        if( newCapacity > arr.size )
            array_internal::_Grow( arr, newCapacity );
    }

}
