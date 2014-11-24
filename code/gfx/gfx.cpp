#include "gfx.h"
#include "gdi/gdi_context.h"
#include <util/common.h>

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
    ctx->clear();
}
void bxGfxContext::frameDraw( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists )
{
    ctx->setCbuffer( _cbuffer_frameData, 0, bxGdi::eALL_STAGES_MASK );
    ctx->setCbuffer( _cbuffer_instanceData, 0, bxGdi::eALL_STAGES_MASK );
        
    ctx->changeRenderTargets( _framebuffer, 1, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );

    bxGfx::InstanceData instanceData;
    int numInstances = 0;

    bxGfxRenderItem_Iterator itemIt( rLists[0] );
    while( itemIt.ok() )
    {
        bxGfxShadingPass shadingPass = itemIt.shadingPass();

        bxGdi::renderSource_enable( ctx, itemIt.renderSource() );
        bxGdi::shaderFx_enable( ctx, shadingPass.fxI, shadingPass.passIndex );

        const int nInstances = itemIt.nWorldMatrices();
        const int nSurfaces = itemIt.nSurfaces();

        const Matrix4* worldMatrices = itemIt.worldMatrices();
        const bxGdiRenderSurface* surfaces = itemIt.surfaces();

        int instancesLeft = nInstances;
        int instancesDrawn = 0;
        while( instancesLeft > 0 )
        {
            const int grab = minOfpair( bxGfx::cMAX_WORLD_MATRICES, instancesLeft );
            for( int imatrix = 0; imatrix < grab; ++imatrix )
            {
                bxGfx::instanceData_setMatrix( &instanceData, imatrix, worldMatrices[ instancesDrawn + imatrix] );
            }

            instancesDrawn += grab;
            instancesLeft -= grab;

            ctx->backend()->updateCBuffer( _cbuffer_instanceData, &instanceData );
            
            if( ctx->indicesBound() )
            {
                for( int isurface = 0; isurface < nSurfaces; ++isurface )
                {
                    const bxGdiRenderSurface& surf = surfaces[isurface];
                    ctx->drawIndexedInstanced( surf.count, surf.begin, grab, 0 );
                }
            }
            else
            {
                for( int isurface = 0; isurface < nSurfaces; ++isurface )
                {
                    const bxGdiRenderSurface& surf = surfaces[isurface];
                    ctx->drawInstanced( surf.count, surf.begin, grab );
                }
            }
        }
        itemIt.next();
    }

    ctx->changeToMainFramebuffer();

    {
        
    }
}
void bxGfxContext::frameEnd( bxGdiContext* ctx  )
{
    ctx->backend()->swap();
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

bxGfxRenderItem_Bucket bxGfxRenderList::surfacesAdd( const bxGdiRenderSurface* surfaces, int count )
{
    if( (_size_surfaces + count) >= _capacity_surfaces )
        return bxGfx::renderItem_invalidBucket();

    const int index = _size_surfaces;

    memcpy( _surfaces + index, surfaces, sizeof(*surfaces) * count );
    _size_surfaces += count;

    bxGfxRenderItem_Bucket bucket;
    bucket.index = u16( index );
    bucket.count = u16( count );
    return bucket;
}

bxGfxRenderItem_Bucket bxGfxRenderList::instancesAdd( const Matrix4* matrices, int count )
{
    if ( _size_instances + count > _capacity_instances )
        return bxGfx::renderItem_invalidBucket();

    const int index = _size_instances;
    _size_instances += count;

    memcpy( _worldMatrices + index, matrices, sizeof( *_worldMatrices ) * count );
    
    bxGfxRenderItem_Bucket bucket;
    bucket.index = u16( index );
    bucket.count = u16( count );
    return bucket;
}

int bxGfxRenderList::itemSubmit( int renderDataIndex, bxGfxRenderItem_Bucket surfaces, bxGfxRenderItem_Bucket instances, u8 renderMask, u8 renderLayer)
{
    SYS_ASSERT( renderDataIndex < _size_renderData );
    if ( renderDataIndex == -1 )
        return -1;

    if ( _size_items >= _capacity_items )
        return -1;

    

    const int renderItemIndex = _size_items++;

    bxGfxRenderItem& item = _items[renderItemIndex];
    item.index_renderData = renderDataIndex;
    item.mask_render = renderMask;
    item.layer = renderLayer;

    item.bucket_surfaces = surfaces;
    item.bucket_instances = instances;

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
    memSize += maxItems * sizeof( bxGdiRenderSurface );
    memSize += maxInstances * sizeof( Matrix4 );

    void* memHandle = BX_MALLOC( allocator, memSize, 16 );
    memset( memHandle, 0, memSize );

    bxBufferChunker chunker( memHandle, memSize );

    bxGfxRenderList* rlist = chunker.add< bxGfxRenderList >();
    new(rlist) bxGfxRenderList();
    rlist->_capacity_renderData = maxItems;
    rlist->_capacity_items = maxItems;
    rlist->_capacity_instances = maxInstances;
    rlist->_capacity_surfaces = maxItems;


    rlist->_localAABBs = chunker.add<bxAABB>( maxItems );
    rlist->_worldMatrices = chunker.add<Matrix4>( maxInstances );
    rlist->_rsources = chunker.add< bxGdiRenderSource*>( maxItems );
    rlist->_shaders = chunker.add< bxGfxShadingPass >( maxItems );
    rlist->_surfaces = chunker.add< bxGdiRenderSurface >( maxItems );
    rlist->_items = chunker.add< bxGfxRenderItem >( maxItems );

    chunker.check();

    return rlist;
}

void bxGfx::renderList_delete(bxGfxRenderList** rList, bxAllocator* allocator)
{
    rList[0]->~bxGfxRenderList();
    BX_FREE0( allocator, rList[0] );
}