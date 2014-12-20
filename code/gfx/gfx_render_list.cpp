#include "gfx_render_list.h"

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/bbox.h>

#include <util/buffer_utils.h>
bxGfxRenderList::bxGfxRenderList()
{
    memset( this, 0, sizeof( *this ) );
}

void bxGfxRenderList::clear()
{
    memset( &_size, 0, sizeof( _size ) );
}

int bxGfxRenderList::renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB )
{
    if ( _size.renderData > _capacity.renderData )
        return -1;

    const int renderDataIndex = _size.renderData++;

    _rsources[renderDataIndex] = rsource;

    bxGfxShadingPass shadingPass;
    shadingPass.fxI = fxI;
    shadingPass.passIndex = passIndex;
    _shaders[renderDataIndex] = shadingPass;

    _localAABBs[renderDataIndex] = localAABB;

    return renderDataIndex;
}

u32 bxGfxRenderList::surfacesAdd( const bxGdiRenderSurface* surfaces, int count )
{
    if ( (_size.surfaces + count) >= _capacity.surfaces )
        return bxGfx::renderItem_invalidBucket();

    const int index = _size.surfaces;

    memcpy( _surfaces + index, surfaces, sizeof( *surfaces ) * count );
    _size.surfaces += count;

    bxGfxRenderItem_Bucket bucket;
    bucket.index = u16( index );
    bucket.count = u16( count );
    return bucket.hash;
}

u32 bxGfxRenderList::instancesAdd( const Matrix4* matrices, int count )
{
    if ( _size.instances + count > _capacity.instances )
        return bxGfx::renderItem_invalidBucket();

    const int index = _size.instances;
    _size.instances += count;

    memcpy( _worldMatrices + index, matrices, sizeof( *_worldMatrices ) * count );

    bxGfxRenderItem_Bucket bucket;
    bucket.index = u16( index );
    bucket.count = u16( count );
    return bucket.hash;
}

int bxGfxRenderList::itemSubmit( int renderDataIndex, u32 surfaces, u32 instances, u8 renderMask, u8 renderLayer )
{
    SYS_ASSERT( renderDataIndex < _size.renderData );
    if ( renderDataIndex == -1 )
        return -1;

    if ( _size.items >= _capacity.items )
        return -1;



    const int renderItemIndex = _size.items++;

    bxGfxRenderItem& item = _items[renderItemIndex];
    item.index_renderData = renderDataIndex;
    item.mask_render = renderMask;
    item.layer = renderLayer;

    item.bucket_surfaces.hash = surfaces;
    item.bucket_instances.hash = instances;

    SYS_ASSERT( item.bucket_surfaces.index + item.bucket_surfaces.count <= _size.surfaces );
    SYS_ASSERT( item.bucket_instances.index + item.bucket_instances.count <= _size.instances );

    return renderItemIndex;
}

void bxGfxRenderList::renderListAppend( bxGfxRenderList* rList )
{
    bxGfxRenderList* tail = this;
    while ( tail->next )
        tail = tail->next;

    tail->next = rList;
}



bxGfxRenderList* bxGfx::renderList_new( int maxItems, int maxInstances, bxAllocator* allocator )
{
    int memSize = 0;
    memSize += sizeof( bxGfxRenderList );
    memSize += maxItems * sizeof( bxGdiRenderSource* );
    memSize += maxItems * sizeof( bxGfxShadingPass );
    memSize += maxItems * sizeof( bxAABB );
    memSize += maxItems * sizeof( bxGfxRenderItem );
    memSize += maxItems * sizeof( bxGdiRenderSurface );
    memSize += maxInstances * sizeof( Matrix4 );

    void* memHandle = BX_MALLOC( allocator, memSize, 16 );
    memset( memHandle, 0, memSize );

    bxBufferChunker chunker( memHandle, memSize );

    bxGfxRenderList* rlist = chunker.add< bxGfxRenderList >();
    new(rlist)bxGfxRenderList();
    rlist->_capacity.renderData = maxItems;
    rlist->_capacity.items = maxItems;
    rlist->_capacity.instances = maxInstances;
    rlist->_capacity.surfaces = maxItems;


    rlist->_localAABBs = chunker.add<bxAABB>( maxItems );
    rlist->_worldMatrices = chunker.add<Matrix4>( maxInstances );
    rlist->_rsources = chunker.add< bxGdiRenderSource*>( maxItems );
    rlist->_shaders = chunker.add< bxGfxShadingPass >( maxItems );
    rlist->_surfaces = chunker.add< bxGdiRenderSurface >( maxItems );
    rlist->_items = chunker.add< bxGfxRenderItem >( maxItems );

    chunker.check();

    return rlist;
}

void bxGfx::renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator )
{
    rList[0]->~bxGfxRenderList();
    BX_FREE0( allocator, rList[0] );
}