#pragma once

#include <vector>

template<typename T>
class vector_t : public std::vector<T>
{
public:
    void fast_erase( u32 pos )
    {
        if( pos > size() )
            return;

        if( pos != size() - 1 )
        {
            (*this)[pos] = back();
        }
        pop_back();
    }
    void ordered_erase( u32 pos )
    {
        if( pos > size() )
            return;

        for( size_t i = pos + 1; i < size(); ++i )
        {
            (*this)[i-1] = (*this)[i];
        }
        pop_back();
    }

};

namespace array
{
    template< typename T > inline T* begin( vector_t<T>& a ) { return ( a.empty() ) ? 0 : &a[0]; }
    template< typename T > inline T* end( vector_t<T>& a )   { return ( a.empty() ) ? 0 : &a[0] + a.size(); }
    template< typename T > inline const T* begin( const vector_t<T>& a ) { return ( a.empty() ) ? 0 : &a[0]; }
    template< typename T > inline const T* end  ( const vector_t<T>& a ) { return ( a.empty() ) ? 0 : &a[0] + a.size(); }
    template< typename T > inline int size( const vector_t<T>& a ) { return (int)a.size(); }
    template< typename T > T&       front   ( vector_t<T>& arr )       { return arrfront(); }
    template< typename T > const T& front   ( const vector_t<T>& arr ) { return arrfront(); }
    template< typename T > T&       back    ( vector_t<T>& arr )       { return arr.back(); }
    template< typename T > const T& back    ( const vector_t<T>& arr ) { return arr.back(); }

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

}///