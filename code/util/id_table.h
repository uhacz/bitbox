#include "containers.h"

template <u32 MAX>
inline id_table_t<MAX>::id_table_t()
    : _freelist( BX_INVALID_ID )
    , _next_id( 0 )
    , _size( 0 )
{
    for( u32 i = 0; i < MAX; i++ )
    {
        _ids[i].id = BX_INVALID_ID;
    }
}

namespace id_table
{
    template <u32 MAX>
    inline id_t create( id_table_t<MAX>& a )
    {
        SYS_ASSERT( a._size < MAX );
        // Obtain a new id
        id_t id;
        id.id = ++a._next_id;

        // Recycle slot if there are any
        if( a._freelist != BX_INVALID_ID )
        {
            id.index = a._freelist;
            a._freelist = a._ids[a._freelist].index;
        }
        else
        {
            id.index = a._size;
        }

        a._ids[id.index] = id;
        a._size++;

        return id;
    }

    template <u32 MAX>
    inline void destroy( id_table_t<MAX>& a, id_t id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdTable does not have ID: %d,%d", id.id, id.index );

        a._ids[id.index].id = BX_INVALID_ID;
        a._ids[id.index].index = a._freelist;
        a._freelist = id.index;
        a._size--;
    }

    template <u32 MAX>
    inline bool has( const id_table_t<MAX>& a, id_t id )
    {
        return id.index < MAX && a._ids[id.index].id == id.id;
    }

    template <u32 MAX>
    inline u16 size( const id_table_t<MAX>& a )
    {
        return a._size;
    }

    template <u32 MAX>
    inline const id_t* begin( const id_table_t<MAX>& a )
    {
        return a._ids;
    }

    template <u32 MAX>
    inline const id_t* end( const id_table_t<MAX>& a )
    {
        return a._ids + MAX;
    }
} // namespace id_table

