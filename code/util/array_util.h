#pragma once

namespace array
{
    template< typename T >
    struct OpEqual
    {
        OpEqual( T value )
            : _value( value )
        {}
        bool operator () ( const T& a ) const { return a == _value; }
        bool operator () ( T& a ) { return a == _value; }

        T _value;
    };

    template< typename T, typename Top > 
    inline T* find( T* first, T* last, Top op )
    {
        while (first!=last) 
        {
            if (op(*first)) 
            {
                return first;
            }
            ++first;
        }
        return last;
    }

    template< typename T, typename Top > 
    inline int find1( T* first, T* last, Top op )
    {
        T* found = find( first, last, op );
        return ( found < last ) ? int( found - first ) : -1;
    }
}///