#include "gfx.h"

union bxGfxSortKey_Color
{
    u64 hash;
    struct
    {
        u64 rSource : 8;
        u64 shader  : 32;
        u64 depth   : 16;
        u64 layer   : 8;
    };
};
union bxGfxSortKey_Depth
{
    u16 hash;
    struct
    {
        u16 depth;
    };
};

////
////
bxGfxContext::bxGfxContext()
{

}

bxGfxContext::~bxGfxContext()
{

}

bxGfx::Shared bxGfxContext::_shared;
int bxGfxContext::startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    const int fbWidth = 1920;
    const int fbHeight = 1080;

    _cbuffer_frameData = dev->createBuffer( sizeof( bxGfx::FrameData ), bxGdi::eBIND_CONSTANT_BUFFER );
    _cbuffer_instanceData = dev->createBuffer( sizeof( bxGfx::InstanceData ), bxGdi::eBIND_CONSTANT_BUFFER );

    _framebuffer[bxGfx::eFRAMEBUFFER_COLOR] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4, 0, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    
    {
        _shared.shader.utils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "utils" );
        _shared.shader.texUtils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );
    }

    return 0;
}

void bxGfxContext::shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    for( int itexture = 0; itexture < bxGfx::eFRAMEBUFFER_COUNT; ++itexture )
    {
        dev->releaseTexture( &_framebuffer[itexture] );
    }
        
    dev->releaseBuffer( &_cbuffer_instanceData );
    dev->releaseBuffer( &_cbuffer_frameData );
}

void bxGfxContext::frameBegin( bxGdiContext* ctx )
{

}
void bxGfxContext::frameDraw( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists )
{

}
void bxGfxContext::frameEnd( bxGdiContext* ctx  )
{

}

////
////
#include <util/buffer_utils.h>
bxGfxRenderList::bxGfxRenderList()
{
    memset( this, 0, sizeof( *this ) );
}
int bxGfxRenderList::renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB )
{
    if ( _size_renderData > _capacity_renderData )
        return -1;

    const int renderDataIndex = _size_renderData++;
    
    _rsources[renderDataIndex] = rsource;

    bxGfxShadingPass shadingPass;
    shadingPass.fxI = fxI;
    shadingPass.passIndex = passIndex;
    _shaders[renderDataIndex] = shadingPass;

    _localAABBs[renderDataIndex] = localAABB;

    return renderDataIndex;
}

int bxGfxRenderList::itemSubmit(int renderDataIndex, const Matrix4* worldMatrices, int nMatrices, u8 renderMask, u8 renderLayer)
{
    SYS_ASSERT( renderDataIndex < _size_renderData );
    if ( renderDataIndex == -1 )
        return -1;

    if ( _size_items >= _capacity_items )
        return -1;

    if ( _size_instances + nMatrices > _capacity_instances )
        return -1;

    const int renderItemIndex = _size_items++;
    const int instancesIndex = _size_instances;
    _size_instances += nMatrices;

    memcpy( _worldMatrices + instancesIndex, worldMatrices, sizeof( *_worldMatrices ) * nMatrices );

    bxGfxRenderItem& item = _items[renderItemIndex];
    item.index_renderData = renderDataIndex;
    item.index_instances = instancesIndex;
    item.count_instances = nMatrices;
    item.mask_render = renderMask;
    item.layer = renderLayer;

    return renderItemIndex;
}

void bxGfxRenderList::renderListAppend(bxGfxRenderList* rList)
{
    bxGfxRenderList* tail = this;
    while ( tail->next )
        tail = tail->next;

    tail->next = rList;
}

bxGfxRenderList* bxGfx::renderList_new(int maxItems, int maxInstances, bxAllocator* allocator)
{
    int memSize = 0;
    memSize += sizeof(bxGfxRenderList);
    memSize += maxItems * sizeof( bxGdiRenderSource* );
    memSize += maxItems * sizeof( bxGfxShadingPass );
    memSize += maxItems * sizeof( bxAABB );
    memSize += maxItems * sizeof( bxGfxRenderItem );
    memSize += maxInstances * sizeof( Matrix4 );

    void* memHandle = BX_MALLOC( allocator, memSize, 16 );
    memset( memHandle, 0, memSize );

    bxBufferChunker chunker( memHandle, memSize );

    bxGfxRenderList* rlist = chunker.add< bxGfxRenderList >();
    new(rlist) bxGfxRenderList();
    rlist->_capacity_renderData = maxItems;
    rlist->_capacity_items = maxItems;
    rlist->_capacity_instances = maxInstances;


    rlist->_localAABBs = chunker.add<bxAABB>( maxItems );
    rlist->_worldMatrices = chunker.add<Matrix4>( maxInstances );
    rlist->_rsources = chunker.add< bxGdiRenderSource*>( maxItems );
    rlist->_shaders = chunker.add< bxGfxShadingPass >( maxItems );
    rlist->_items = chunker.add< bxGfxRenderItem >( maxItems );

    chunker.check();

    return rlist;
}

void bxGfx::renderList_delete(bxGfxRenderList** rList, bxAllocator* allocator)
{
    rList[0]->~bxGfxRenderList();
    BX_FREE0( allocator, rList[0] );
}