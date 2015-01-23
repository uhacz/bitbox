#pragma once

#include <util/type.h>
#include <util/bbox.h>
#include <gdi/gdi_render_source.h>

#include "gfx_type.h"

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
    bxGfxRenderItem_Iterator( bxGfxRenderList* rList )
        : _rList( rList )
        , _currentItem( rList->_items )
        , _endItem( rList->_items + rList->_size.items )
    {}

    int ok() const { return _currentItem < _endItem; }
    void next() { ++_currentItem; }

    bxGdiRenderSource* renderSource()       { return _rList->_rsources[_currentItem->index_renderData]; }
    bxGfxShadingPass   shadingPass() const { return _rList->_shaders[_currentItem->index_renderData]; }
    const bxAABB&      aabb() const { return _rList->_localAABBs[_currentItem->index_renderData]; }

    const bxGdiRenderSurface* surfaces() const { return _rList->_surfaces + _currentItem->bucket_surfaces.index; }
    int nSurfaces() const { return _currentItem->bucket_surfaces.count; }

    const Matrix4*     worldMatrices() const { return _rList->_worldMatrices + _currentItem->bucket_instances.index; }
    int                nWorldMatrices() const { return _currentItem->bucket_instances.count; }

    u8                 renderMask() const { return _currentItem->mask_render; }
    u8                 renderLayer() const { return _currentItem->layer; }

    bxGfxRenderList* _rList;
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
    struct Entry
    {
        Tkey key;
        i32 offset_rList;
        i32 offset_rItem;
    };

    struct EntryCmp_Ascending{
        bool operator() ( const Entry& a, const Entry& b ) const { return a.key < b.key; }
    };
    struct EntryCmp_Descending{
        bool operator() ( const Entry& a, const Entry& b ) const { return a.key > b.key; }
    };

    Entry* _sortData;
    
    i32 _size_sortData;
    i32 _capacity_sortData;

    int add( const Tkey key, bxGfxRenderList* rList, bxGfxRenderItem* rItem )
    {
        if( _size_sortData >= _capacity_sortData )
            return -1;

        const int index = _size_sortData++;

        Entry& e = _sortData[index];
        e.key = key;
        e.offset_rList = TYPE_POINTER_GET_OFFSET( &e.offset_rList, rList );
        e.offset_rItem = TYPE_POINTER_GET_OFFSET( &e.offset_rItem, rItem );
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
    
    bxGfxSortList()
        : _sortData(0)
        , _size_sortData(0)
        , _capacity_sortData(0)
    {}
};

namespace bxGfx
{
    template< class Tkey >
    bxGfxSortList<Tkey>* sortList_new( int maxItems, bxAllocator* allocator )
    {
        
    }
    
    template< class Tkey >
    void sortList_delete( bxGfxSortList<Tkey>** rList, bxAllocator* allocator )
    {
        
    }
}///