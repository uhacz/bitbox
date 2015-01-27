#pragma once

#include <util/type.h>
#include <util/memory.h>
#include "gfx_type.h"
#include <gdi/gdi_context.h>
#include <algorithm>

struct bxGfxRenderList;
struct bxGfxCamera;

template< class Tkey >
struct bxGfxSortList
{
#pragma pack( push, 1 )
    struct Entry
    {
        Tkey key;
        u32 rItemIndex;
        const bxGfxRenderList* rList;
    };
#pragma pack(pop)

    struct EntryCmp_Ascending{
        bool operator() ( const Entry& a, const Entry& b ) const { return a.key.hash < b.key.hash; }
    };
    struct EntryCmp_Descending{
        bool operator() ( const Entry& a, const Entry& b ) const { return a.key.hash > b.key.hash; }
    };

    Entry* _sortData;

    i32 _size_sortData;
    i32 _capacity_sortData;

    int add( const Tkey key, const bxGfxRenderList* rList, u32 rItemIndex )
    {
        if( _size_sortData >= _capacity_sortData )
            return -1;

        const int index = _size_sortData++;

        Entry& e = _sortData[index];
        e.key = key;
        e.rItemIndex = rItemIndex;
        e.rList = rList;
        return index;
    }

    void sortAscending()
    {
        std::sort( _sortData, _sortData + _size_sortData, EntryCmp_Ascending() );
    }
    void sortDescending()
    {
        std::sort( _sortData, _sortData + _size_sortData, EntryCmp_Descending() );
    }
    void clear()
    {
        _size_sortData = 0;  
    }
};

union bxGfxSortKey_Color
{
    u64 hash;
    struct
    {
        u64 mesh     : 24;
        u64 shader   : 32;
        u64 layer    : 8;
    };
};

struct bxGfxSortKey_Depth
{
    u16 depth;
};


typedef bxGfxSortList< bxGfxSortKey_Color > bxGfxSortList_Color;
typedef bxGfxSortList< bxGfxSortKey_Depth > bxGfxSortList_Depth;


namespace bxGfx
{
    template< class Tlist >
    Tlist* sortList_new( int maxItems, bxAllocator* allocator )
    {
        int memorySize = 0;
        memorySize += sizeof( Tlist );
        memorySize += maxItems * sizeof( Tlist::Entry );

        void* memoryHandle = BX_MALLOC( allocator, memorySize, 16 );
        memset( memoryHandle, 0x00, memorySize );

        bxBufferChunker chunker( memoryHandle, memorySize );

        Tlist* sList = chunker.add< Tlist >();
        sList->_sortData = chunker.add< Tlist::Entry >( maxItems );

        chunker.check();

        sList->_size_sortData = 0;
        sList->_capacity_sortData = maxItems;

        return sList;
    }

    template< class Tlist >
    void sortList_delete( Tlist** sList, bxAllocator* allocator )
    {
        BX_FREE0( allocator, sList[0] );
    }

    template< class Tlist >
    void submitSortList( bxGdiContext* ctx, bxGdiBuffer cbuffer_instanceData, Tlist* sList )
    {
        const int nItems = sList->_size_sortData;
        bxGfx::InstanceData instanceData;
        memset( &instanceData, 0x00, sizeof( instanceData ) );
        for( int iitem = 0; iitem < nItems; ++iitem )
        {
            Tlist::Entry e = sList->_sortData[iitem];

            bxGfxRenderItem_Iterator itemIt( e.rList, e.rItemIndex );
            submitRenderItem( ctx, &instanceData, cbuffer_instanceData, itemIt );
        }
    }
}///

namespace bxGfx
{
    void sortList_computeColor( bxGfxSortList_Color* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask = bxGfx::eRENDER_MASK_COLOR );
    void sortList_computeDepth( bxGfxSortList_Depth* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask = bxGfx::eRENDER_MASK_DEPTH );
 
}