#include "containers.h"

#define BX_ID_ARRAY_T_DEF u32 MAX, typename Tid
#define BX_ID_ARRAY_T_ARG MAX, Tid

namespace id_array
{
    template <BX_ID_ARRAY_T_DEF>
    inline Tid create( id_array_t<BX_ID_ARRAY_T_ARG>& a )
    {
        SYS_ASSERT_TXT( a._size < MAX, "Object list full" );

        // Obtain a new id
        Tid id;
        id.id = ++a._next_id;

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
        //a._objects[a._size] = object;
        a._size++;

        return id;
    }

    template <BX_ID_ARRAY_T_DEF>
    inline void destroy( id_array_t<BX_ID_ARRAY_T_ARG>& a, Tid id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );

        a._sparse[id.index].id = BX_INVALID_ID;
        a._sparse[id.index].index = a._freelist;
        a._freelist = id.index;

        // Swap with last element
        const u32 last = a._size - 1;
        SYS_ASSERT_TXT( last >= a._sparse_to_dense[id.index], "Swapping with previous item" );
        //a._objects[a._sparse_to_dense[id.index]] = a._objects[last];

        // Update tables
        u16 std = a._sparse_to_dense[id.index];
        u16 dts = a._dense_to_sparse[last];
        a._sparse_to_dense[dts] = std;
        a._dense_to_sparse[std] = dts;
        a._size--;
    }

    template <BX_ID_ARRAY_T_DEF>
    inline void destroyAll( id_array_t<BX_ID_ARRAY_T_ARG>& a )
    {
        while( a._size )
        {
            const u32 last = a._size - 1;
            Tid lastId = a._sparse[a._dense_to_sparse[last]];
            destroy( a, lastId );
        }
    }

    template <BX_ID_ARRAY_T_DEF>
    inline int index( const id_array_t<BX_ID_ARRAY_T_ARG>& a, const Tid& id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );

        return (int)a._sparse_to_dense[id.index];
    }

    template <BX_ID_ARRAY_T_DEF>
    inline bool has( const id_array_t<BX_ID_ARRAY_T_ARG>& a, Tid id )
    {
        return id.index < MAX && a._sparse[id.index].id == id.id;
    }

    template <BX_ID_ARRAY_T_DEF>
    inline u32 size( const id_array_t<BX_ID_ARRAY_T_ARG>& a )
    {
        return a._size;
    }


    //template <u32 MAX>
    //inline T* begin( id_array_t<BX_ID_ARRAY_T_ARG>& a )
    //{
    //    return a._objects;
    //}

    //template <u32 MAX>
    //inline const T* begin( const id_array_t<BX_ID_ARRAY_T_ARG>& a )
    //{
    //    return a._objects;
    //}

    //template <u32 MAX>
    //inline u16* end( id_array_t<BX_ID_ARRAY_T_ARG>& a )
    //{
    //    return a._objects + a._size;
    //}

    //template <u32 MAX>
    //inline const u16* end( const id_array_t<BX_ID_ARRAY_T_ARG>& a )
    //{
    //    return a._objects + a._size;
    //}

}///
