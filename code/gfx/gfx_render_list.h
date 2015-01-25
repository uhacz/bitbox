#pragma once

#include <util/type.h>
#include <util/bbox.h>
#include <gdi/gdi_render_source.h>

#include "gfx_type.h"
#include <algorithm>
#include <util/buffer_utils.h>
////
////
struct bxGfxRenderEffect
{
    virtual ~bxGfxRenderEffect() {}
    virtual void draw() = 0;
};

struct bxGfxShadingPass
{
    bxGdiShaderFx_Instance* fxI;
    i32 passIndex;
};

union bxGfxRenderItem_Bucket
{
    u32 hash;
    struct
    {
        u16 index;
        u16 count;
    };
};

struct bxGfxRenderItem
{
    u16 index_renderData;
    u8 mask_render;
    u8 layer;
    bxGfxRenderItem_Bucket bucket_surfaces;
    bxGfxRenderItem_Bucket bucket_instances;
};
namespace bxGfx
{
    inline u32 renderItem_invalidBucket() { return 0xFFFFFFFF; }
    inline int rebderItem_isValidBucket( const bxGfxRenderItem_Bucket b ) { return b.hash != 0xFFFFFFFF; }

}///

////
////
struct BIT_ALIGNMENT_16 bxGfxRenderList
{
    //// renderData
    bxGdiRenderSource** _rsources;
    bxGfxShadingPass* _shaders;
    bxAABB* _localAABBs;

    /// surfaces
    bxGdiRenderSurface* _surfaces;

    //// items
    bxGfxRenderItem* _items;

    //// instances
    Matrix4* _worldMatrices;

    struct
    {
        i32 renderData;
        i32 items;
        i32 surfaces;
        i32 instances;
    } _capacity;

    struct
    {
        i32 renderData;
        i32 items;
        i32 surfaces;
        i32 instances;
    } _size;

    bxGfxRenderList* next;

    int renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB );
    u32 surfacesAdd( const bxGdiRenderSurface* surfaces, int count );
    u32 instancesAdd( const Matrix4* matrices, int count );
    int itemSubmit( int renderDataIndex, u32 surfaces, u32 instances, u8 renderMask = bxGfx::eRENDER_MASK_ALL, u8 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );

    void renderListAppend( bxGfxRenderList* rList );

    void clear();
    bxGfxRenderList();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxGfxRenderListItemDesc
{
    bxAABB _localAABB;
    bxGdiRenderSource* _rsource;
    bxGdiShaderFx_Instance* _fxI;
    i32 _passIndex;
    i32 _dataIndex;

    bxGfxRenderListItemDesc( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB )
        : _localAABB( localAABB )
        , _rsource( rsource )
        , _fxI( fxI )
        , _passIndex( passIndex )
        , _dataIndex( -1 )
    {}

    void setRenderSource( bxGdiRenderSource* rsource )
    {
        _rsource = rsource;
        _dataIndex = -1;
    }
    void setShader( bxGdiShaderFx_Instance* fxI, int passIndex )
    {
        _fxI = fxI;
        _passIndex = passIndex;
        _dataIndex = -1;
    }
    void setLocalAABB( const bxAABB& aabb )
    {
        _localAABB = aabb;
        _dataIndex = -1;
    }
};
namespace bxGfx
{
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, int topology, const Matrix4* matrices, int nMatrices )
    {
        if ( itemDesc->_dataIndex < 0 )
        {
            itemDesc->_dataIndex = rList->renderDataAdd( itemDesc->_rsource, itemDesc->_fxI, itemDesc->_passIndex, itemDesc->_localAABB );
        }
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( itemDesc->_rsource, topology );
        u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
        u32 instanceIndex = rList->instancesAdd( matrices, nMatrices );
        rList->itemSubmit( itemDesc->_dataIndex, surfaceIndex, instanceIndex );
    }
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, int topology, const Matrix4& matrix )
    {
        renderList_pushBack( rList, itemDesc, topology, &matrix, 1 );
    }
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, const bxGdiRenderSurface& surf, const Matrix4& matrix )
    {
        if ( itemDesc->_dataIndex < 0 )
        {
            itemDesc->_dataIndex = rList->renderDataAdd( itemDesc->_rsource, itemDesc->_fxI, itemDesc->_passIndex, itemDesc->_localAABB );
        }
        u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
        u32 instanceIndex = rList->instancesAdd( &matrix, 1 );
        rList->itemSubmit( itemDesc->_dataIndex, surfaceIndex, instanceIndex );
    }
}///

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxGfxRenderItem_Iterator
{
    bxGfxRenderItem_Iterator( const bxGfxRenderList* rList, u32 beginItemIndex = 0 )
        : _rList( rList )
        , _currentItem( rList->_items + beginItemIndex )
        , _endItem( rList->_items + rList->_size.items )
    {}

    int ok   () const { return _currentItem < _endItem; }
    void next()       { ++_currentItem; }

    bxGdiRenderSource* renderSource()       { return _rList->_rsources[_currentItem->index_renderData]; }
    bxGfxShadingPass   shadingPass () const { return _rList->_shaders[_currentItem->index_renderData]; }
    const bxAABB&      aabb        () const { return _rList->_localAABBs[_currentItem->index_renderData]; }

    const bxGdiRenderSurface* surfaces () const { return _rList->_surfaces + _currentItem->bucket_surfaces.index; }
    int                       nSurfaces() const { return _currentItem->bucket_surfaces.count; }

    const Matrix4* worldMatrices () const { return _rList->_worldMatrices + _currentItem->bucket_instances.index; }
    int            nWorldMatrices() const { return _currentItem->bucket_instances.count; }

    u8 renderMask () const { return _currentItem->mask_render; }
    u8 renderLayer() const { return _currentItem->layer; }

    u32 itemIndex() const { SYS_ASSERT( (_currentItem - _rList->_items) < 0xFFFFFFFF );  return u32( _currentItem - _rList->_items ); }

    const bxGfxRenderList* _rList;
    bxGfxRenderItem* _currentItem;
    bxGfxRenderItem* _endItem;
};


namespace bxGfx
{
    bxGfxRenderList* renderList_new( int maxItems, int maxInstances, bxAllocator* allocator );
    void renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator );
}///

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

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
        //u64 depth  : 16;
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
}///


namespace bxGfx
{
    void sortList_computeColor( bxGfxSortList_Color* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask = bxGfx::eRENDER_MASK_COLOR );
    void sortList_computeDepth( bxGfxSortList_Depth* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask = bxGfx::eRENDER_MASK_DEPTH );
}