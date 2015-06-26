#include "containers.h"

template <typename T, u32 MAX>
inline id_array_t<MAX, T>::id_array_t()
    : _freelist( BX_INVALID_ID )
    , _next_id( 0 )
    , _size( 0 )
{
    for( u32 i = 0; i < MAX; i++ )
    {
        _sparse[i].id = BX_INVALID_ID;
    }
}

template <typename T, u32 MAX>
inline T& id_array_t<MAX, T>::operator[]( u32 i )
{
    SYS_ASSERT_TXT( i < _size, "Index out of bounds" );
    return _objects[i];
}

template <typename T, u32 MAX>
inline const T& id_array_t<MAX, T>::operator[]( u32 i ) const
{
    SYS_ASSERT_TXT( i < _size, "Index out of bounds" );
    return _objects[i];
}

namespace id_array
{
    template <typename T, u32 MAX>
    inline id_t create( id_array_t<MAX, T>& a, const T& object )
    {
        SYS_ASSERT_TXT( a._size < MAX, "Object list full" );

        // Obtain a new id
        id_t id;
        id.id = a._next_id++;

        // Recycle slot if there are any
        if( a._freelist != BX_INVALID_ID )
        {
            id.index = a._freelist;
            a._freelist = a._sparse[a._freelist].index;
        }
        else
        {
            id.index = a._size;
        }

        a._sparse[id.index] = id;
        a._sparse_to_dense[id.index] = a._size;
        a._dense_to_sparse[a._size] = id.index;
        a._objects[a._size] = object;
        a._size++;

        return id;
    }

    template <typename T, u32 MAX>
    inline void destroy( id_array_t<MAX, T>& a, id_t id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );

        a._sparse[id.index].id = BX_INVALID_ID;
        a._sparse[id.index].index = a._freelist;
        a._freelist = id.index;

        // Swap with last element
        const u32 last = a._size - 1;
        SYS_ASSERT_TXT( last >= a._sparse_to_dense[id.index], "Swapping with previous item" );
        a._objects[a._sparse_to_dense[id.index]] = a._objects[last];

        // Update tables
        u16 std = a._sparse_to_dense[id.index];
        u16 dts = a._dense_to_sparse[last];
        a._sparse_to_dense[dts] = std;
        a._dense_to_sparse[std] = dts;
        a._size--;
    }

    template <typename T, u32 MAX>
    inline T& get( id_array_t<MAX, T>& a, const id_t& id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );

        return a._objects[a._sparse_to_dense[id.index]];
    }

    template <typename T, u32 MAX>
    inline bool has( const id_array_t<MAX, T>& a, id_t id )
    {
        return id.index < MAX && a._sparse[id.index].id == id.id;
    }

    template <typename T, u32 MAX>
    inline u32 size( const id_array_t<MAX, T>& a )
    {
        return a._size;
    }

    template <typename T, u32 MAX>
    inline T* begin( id_array_t<MAX, T>& a )
    {
        return a._objects;
    }

    template <typename T, u32 MAX>
    inline const T* begin( const id_array_t<MAX, T>& a )
    {
        return a._objects;
    }

    template <typename T, u32 MAX>
    inline T* end( id_array_t<MAX, T>& a )
    {
        return a._objects + a._size;
    }

    template <typename T, u32 MAX>
    inline const T* end( const id_array_t<MAX, T>& a )
    {
        return a._objects + a._size;
    }

}///
