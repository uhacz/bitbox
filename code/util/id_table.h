#pragma once

#include "containers.h"

#define BX_ID_TABLE_T_DEF u32 MAX, typename Tid
#define BX_ID_TABLE_T_ARG MAX, Tid

template <BX_ID_TABLE_T_DEF>
inline id_table_t<BX_ID_TABLE_T_ARG>::id_table_t()
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
    template <BX_ID_TABLE_T_DEF>
    inline Tid create( id_table_t<BX_ID_TABLE_T_ARG>& a )
    {
        SYS_ASSERT( a._size < MAX );
        // Obtain a new id
        Tid id;
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

    template <BX_ID_TABLE_T_DEF>
    inline void destroy( id_table_t<BX_ID_TABLE_T_ARG>& a, Tid id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdTable does not have ID: %d,%d", id.id, id.index );

        a._ids[id.index].id = BX_INVALID_ID;
        a._ids[id.index].index = a._freelist;
        a._freelist = id.index;
        a._size--;
    }

    template <BX_ID_TABLE_T_DEF>
    inline bool has( const id_table_t<BX_ID_TABLE_T_ARG>& a, Tid id )
    {
        return id.index < MAX && a._ids[id.index].id == id.id;
    }

    template <BX_ID_TABLE_T_DEF>
    inline u16 size( const id_table_t<BX_ID_TABLE_T_ARG>& a )
    {
        return a._size;
    }

    template <BX_ID_TABLE_T_DEF>
    inline const Tid* begin( const id_table_t<BX_ID_TABLE_T_ARG>& a )
    {
        return a._ids;
    }

    template <BX_ID_TABLE_T_DEF>
    inline const Tid* end( const id_table_t<BX_ID_TABLE_T_ARG>& a )
    {
        return a._ids + MAX;
    }
} // namespace id_table

