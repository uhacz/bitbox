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

namespace bxGfx
{
    void frameData_fill( FrameData* frameData, const bxGfxCamera& camera, int rtWidth, int rtHeight )
    {
        frameData->viewMatrix = camera.matrix.view;
        frameData->projMatrix = camera.matrix.proj;
        frameData->viewProjMatrix = camera.matrix.viewProj;
        frameData->cameraWorldMatrix = camera.matrix.world;

        const float fov = camera.params.fov();
        const float aspect = camera.params.aspect();
        frameData->cameraParams = Vector4( fov, aspect, camera.params.zNear, camera.params.zFar );
        {
            const float a = camera.matrix.proj.getElem( 0, 0 ).getAsFloat();//getCol0().getX().getAsFloat();
            const float b = camera.matrix.proj.getElem( 1, 1 ).getAsFloat();//getCol1().getY().getAsFloat();
            const float c = camera.matrix.proj.getElem( 2, 2 ).getAsFloat();//getCol2().getZ().getAsFloat();
            const float d = camera.matrix.proj.getElem( 3, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            frameData->projParams = Vector4( 1.f/a, 1.f/b, c, -d );
        }

        frameData->eyePos = Vector4( camera.matrix.worldEye(), oneVec );
        frameData->viewDir = Vector4( camera.matrix.worldDir(), zeroVec );
        frameData->renderTargetSizeRcp = Vector4( 1.f / float(rtWidth), 1.f / float(rtHeight), 0.f, 0.f );
    }
}///

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
    
    {//// fullScreenQuad
        const float vertices[] = 
        {
           -1.f, -1.f, 0.f, 0.f, 0.f,
            1.f, -1.f, 0.f, 1.f, 0.f,
            1.f,  1.f, 0.f, 1.f, 1.f,

           -1.f, -1.f, 0.f, 0.f, 0.f,
            1.f,  1.f, 0.f, 1.f, 1.f,
           -1.f,  1.f, 0.f, 0.f, 1.f,
        };
        bxGdiVertexStreamDesc vsDesc;
        vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 );
        vsDesc.addBlock( bxGdi::eSLOT_TEXCOORD0, bxGdi::eTYPE_FLOAT, 2 );
        bxGdiVertexBuffer vBuffer = dev->createVertexBuffer( vsDesc, 6, vertices );
        
        _shared.rsource.fullScreenQuad = bxGdi::renderSource_new( 1 );
        bxGdi::renderSource_setVertexBuffer( _shared.rsource.fullScreenQuad, vBuffer, 0 );
    }
    {//// poly shapes
        bxPolyShape polyShape;
        bxPolyShape_createBox( &polyShape, 1 );
        _shared.rsource.box = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        bxPolyShape_createShpere( &polyShape, 4 );
        _shared.rsource.sphere = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
        bxPolyShape_deallocateShape( &polyShape );
    }

    return 0;
}

void bxGfxContext::shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    {
        bxGdi::renderSource_releaseAndFree( dev, &_shared.rsource.sphere );
        bxGdi::renderSource_releaseAndFree( dev, &_shared.rsource.box );
        bxGdi::renderSource_releaseAndFree( dev, &_shared.rsource.fullScreenQuad );
    }
    {
        bxGdi::shaderFx_releaseWithInstance( dev, &_shared.shader.texUtils );
        bxGdi::shaderFx_releaseWithInstance( dev, &_shared.shader.utils );
    }
    
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
    bxGfx::FrameData fdata;
    bxGfx::frameData_fill( &fdata, camera, _framebuffer[0].width, _framebuffer[0].height );
    ctx->backend()->updateCBuffer( _cbuffer_frameData, &fdata );

    ctx->setCbuffer( _cbuffer_frameData, 0, bxGdi::eALL_STAGES_MASK );
    ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eALL_STAGES_MASK );
        
    ctx->changeRenderTargets( _framebuffer, 1, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, _framebuffer[0].width, _framebuffer[0].height ) );

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
            const int grab = minOfPair( bxGfx::cMAX_WORLD_MATRICES, instancesLeft );
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
                    ctx->setTopology( surf.topology );
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
        bxGdiTexture colorTexture = _framebuffer[bxGfx::eFRAMEBUFFER_COLOR];
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();
        bxGdiViewport viewport = bxGfx::cameraParams_viewport( camera.params, backBuffer.width, backBuffer.height, colorTexture.width, colorTexture.height );

        ctx->setViewport( viewport );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );

        bxGdiShaderFx_Instance* fxI = _shared.shader.texUtils;
        fxI->setTexture( "gtexture", colorTexture );
        fxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );

        bxGdi::renderSource_enable( ctx, _shared.rsource.fullScreenQuad );
        bxGdi::shaderFx_enable( ctx, fxI, "copy_rgba" );
        ctx->draw( _shared.rsource.fullScreenQuad->vertexBuffers->numElements, 0 );
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
    if( (_size.surfaces + count) >= _capacity.surfaces )
        return bxGfx::renderItem_invalidBucket();

    const int index = _size.surfaces;

    memcpy( _surfaces + index, surfaces, sizeof(*surfaces) * count );
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

int bxGfxRenderList::itemSubmit( int renderDataIndex, u32 surfaces, u32 instances, u8 renderMask, u8 renderLayer)
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

void bxGfx::renderList_delete(bxGfxRenderList** rList, bxAllocator* allocator)
{
    rList[0]->~bxGfxRenderList();
    BX_FREE0( allocator, rList[0] );
}