#include "gfx_mesh_renderer.h"
#include "gfx.h"
#include "gfx_camera.h"
#include "gfx_lights.h"

#include <gdi/gdi_context.h>
#include <gdi/gdi_shader.h>

#include <util/float16.h>
#include <util/random.h>
#include <util/time.h>

////
////
bxGfxContext::bxGfxContext()
    : _sortList_color( 0 )
    , _sortList_depth( 0 )
{
    _scene_zRange[0] = -FLT_MAX;
    _scene_zRange[1] = FLT_MAX;

}

bxGfxContext::~bxGfxContext()
{}

int bxGfxContext::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    const int fbWidth = 1920;
    const int fbHeight = 1080;
    //const int fbWidth = 1280;
    //const int fbHeight = 720;

    _cbuffer_frameData = dev->createConstantBuffer( sizeof( bxGfx::FrameData ) );
    _cbuffer_instanceData = dev->createConstantBuffer( sizeof( bxGfx::InstanceData ) );

    _framebuffer[bxGfx::eFRAMEBUFFER_COLOR] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_SWAP] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_NORMAL_VS] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_UBYTE, 1, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME] = dev->createTexture2D( fbWidth / 2, fbHeight / 2, 1, bxGdiFormat( bxGdi::eTYPE_UBYTE, 1, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );



    const int nSortItems = 1024 * 8;
    _sortList_color = bxGfx::sortList_new<bxGfxSortList_Color>( nSortItems, bxDefaultAllocator() );
    _sortList_depth = bxGfx::sortList_new<bxGfxSortList_Depth>( nSortItems, bxDefaultAllocator() );
    _sortList_shadow = bxGfx::sortList_new<bxGfxSortList_Shadow>( nSortItems, bxDefaultAllocator() );

    _shadows._Startup( dev, resourceManager );

    return 0;
}

void bxGfxContext::shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    _shadows._Shutdown( dev, resourceManager );

    bxGfx::sortList_delete( &_sortList_depth, bxDefaultAllocator() );
    bxGfx::sortList_delete( &_sortList_color, bxDefaultAllocator() );


    for ( int itexture = 0; itexture < bxGfx::eFRAMEBUFFER_COUNT; ++itexture )
    {
        dev->releaseTexture( &_framebuffer[itexture] );
    }

    dev->releaseBuffer( &_cbuffer_instanceData );
    dev->releaseBuffer( &_cbuffer_frameData );
}

void bxGfxContext::frame_begin( bxGdiContext* ctx )
{
    ctx->clear();

    _sortList_color->clear();
    _sortList_depth->clear();
    _sortList_shadow->clear();
}

void bxGfxContext::frame_zPrepass( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists )
{
    _scene_zRange[0] = FLT_MAX;
    _scene_zRange[1] = -FLT_MAX;
    for ( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeDepth( _sortList_depth, _scene_zRange, *rLists[ilist], camera );
    }
    _sortList_depth->sortAscending();

    bindCamera( ctx, camera, _framebuffer->width, _framebuffer->height );
    ctx->changeRenderTargets( &_framebuffer[bxGfx::eFRAMEBUFFER_NORMAL_VS], 1, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, _framebuffer[0].width, _framebuffer[0].height ) );
    ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eSTAGE_MASK_VERTEX );

    bxGfx_GlobalResources* gr = bxGfx::globalResources();
    bxGdi::shaderFx_enable( ctx, gr->fx.utils, "z_prepass" );
    bxGfx::submitSortList( ctx, _cbuffer_instanceData, _sortList_depth, bxGfx::eRENDER_ITEM_USE_STREAMS );
}

void bxGfxContext::frame_drawShadows( bxGdiContext* ctx, bxGfxRenderList** rLists, int numLists, const bxGfxCamera& camera, const bxGfxLights& lights )
{
    const bxGfxLight_Sun sunLight = lights.sunLight();
    const Vector3 sunLightDirection( xyz_to_m128( sunLight.dir.xyz ) );

    //const u16 minZ16 = _sortList_depth->_sortData[0].key.depth;
    //const u16 maxZ16 = _sortList_depth->_sortData[_sortList_depth->_size_sortData-1].key.depth;
    const float sceneZRange[2] =
    {
        maxOfPair( _scene_zRange[0], camera.params.zNear ),
        minOfPair( _scene_zRange[1], camera.params.zFar ),
        //half_to_float( fromU16( minZ16 ) ).f,
        //half_to_float( fromU16( maxZ16 ) ).f,
    };

    bxGfxCamera cameraForCascades = camera;
    cameraForCascades.params.zNear = sceneZRange[0];
    cameraForCascades.params.zFar = sceneZRange[1];
    bxGfx::cameraMatrix_compute( &cameraForCascades.matrix, cameraForCascades.params, cameraForCascades.matrix.world, 0, 0 );

    float depthSplits[bxGfx::eSHADOW_NUM_CASCADES];
    bxGfxShadows_Cascade cascades[bxGfx::eSHADOW_NUM_CASCADES];
    memset( cascades, 0x00, sizeof( cascades ) );

    bxGfx::shadows_splitDepth( depthSplits, cameraForCascades.params, sceneZRange, 0.75f );
    bxGfx::shadows_computeCascades( cascades, depthSplits, cameraForCascades, sceneZRange, sunLightDirection );

    for ( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeShadow( _sortList_shadow, *rLists[ilist], cascades, bxGfx::eSHADOW_NUM_CASCADES );
    }

    _sortList_shadow->sortAscending();



    {
        bxGfx::InstanceData instanceData;
        memset( &instanceData, 0x00, sizeof( instanceData ) );

        bxGdiViewport viewports[bxGfx::eSHADOW_NUM_CASCADES];
        for ( int i = 0; i < bxGfx::eSHADOW_NUM_CASCADES; ++i )
        {
            viewports[i] = bxGdiViewport( i * bxGfx::eSHADOW_CASCADE_SIZE, 0, bxGfx::eSHADOW_CASCADE_SIZE, bxGfx::eSHADOW_CASCADE_SIZE );
        }

        bxGdiTexture depthTexture = _shadows._depthTexture;
        bxGdiShaderFx_Instance* shadowsFxI = _shadows._fxI;
        bxGfxSortList_Shadow* sList = _sortList_shadow;

        ctx->changeRenderTargets( 0, 0, depthTexture );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );

        bxGdi::shaderFx_enable( ctx, shadowsFxI, "depth" );

        u8 currentCascade = 0xff;
        for ( int iitem = 0; iitem < sList->_size_sortData; ++iitem )
        {
            const bxGfxSortList_Shadow::Entry& e = sList->_sortData[iitem];
            const bxGfxSortKey_Shadow key = e.key;

            SYS_ASSERT( key.cascade < bxGfx::eSHADOW_NUM_CASCADES );

            if ( currentCascade != key.cascade )
            {
                //SYS_ASSERT( currentCascade < key.cascade );
                currentCascade = key.cascade;
                const bxGfxShadows_Cascade& cascade = cascades[currentCascade];

                bxGfxCamera cascadeCamera;
                cascadeCamera.matrix.view = cascade.view;
                cascadeCamera.matrix.proj = cascade.proj;
                //cascadeCamera.matrix.viewProj = cascade.proj * cascade.view;
                cascadeCamera.matrix.world = inverse( cascade.view );
                cascadeCamera.params.zNear = cascade.zNear_zFar.getX().getAsFloat();
                cascadeCamera.params.zFar = cascade.zNear_zFar.getY().getAsFloat();

                bindCamera( ctx, cascadeCamera, bxGfx::eSHADOW_CASCADE_SIZE, bxGfx::eSHADOW_CASCADE_SIZE );
                ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eSTAGE_MASK_VERTEX );
                ctx->setViewport( viewports[key.cascade] );
            }

            bxGfxRenderItem_Iterator itemIt( e.rList, e.rItemIndex );
            submitRenderItem( ctx, &instanceData, _cbuffer_instanceData, itemIt, bxGfx::eRENDER_ITEM_USE_STREAMS );
        }

        {

            bxGdiTexture shadowsTexture = _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS];
            bxGdiTexture shadowsVolumeTexture = _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME];


            Matrix4 worldToShadowSpace[bxGfx::eSHADOW_NUM_CASCADES];
            Matrix4 viewToShadowSpace[bxGfx::eSHADOW_NUM_CASCADES];
            Vector4 clipPlanes[bxGfx::eSHADOW_NUM_CASCADES];

            const Matrix4 sc = Matrix4::scale( Vector3( 0.5f, 0.5f, 0.5f ) );
            const Matrix4 tr = Matrix4::translation( Vector3( 1.f, 1.f, 1.f ) );
            for ( int i = 0; i < bxGfx::eSHADOW_NUM_CASCADES; ++i )
            {
                const bxGfxShadows_Cascade& cascade = cascades[i];
                worldToShadowSpace[i] = (sc * tr * cascade.proj) * cascade.view;
                viewToShadowSpace[i] = worldToShadowSpace[i] * camera.matrix.world;

                clipPlanes[i] = mulPerElem( cascade.zNear_zFar, Vector4( -1.f, -1.f, 1.f, 1.f ) );
                clipPlanes[i].setZ( _shadows._params.bias );
                clipPlanes[i].setW( _shadows._params.normalOffsetScale[i] );
            }
            shadowsFxI->setUniform( "worldToShadowSpace", worldToShadowSpace );
            shadowsFxI->setUniform( "viewToShadowSpace", viewToShadowSpace );
            shadowsFxI->setUniform( "clipPlanes_bias_nOffset", clipPlanes );
            shadowsFxI->setUniform( "lightDirectionWS", sunLightDirection );
            shadowsFxI->setUniform( "occlusionTextureSize", float2_t( shadowsTexture.width, shadowsTexture.height ) );
            shadowsFxI->setUniform( "shadowMapSize", float2_t( depthTexture.width, depthTexture.height ) );

            shadowsFxI->setUniform( "depthBias", _shadows._params.bias );
            shadowsFxI->setUniform( "useNormalOffset", _shadows._params.flag_useNormalOffset );

            shadowsFxI->setTexture( "shadowMap", _shadows._depthTexture );
            shadowsFxI->setTexture( "sceneDepthTex", _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
            shadowsFxI->setTexture( "normalsVS", _framebuffer[bxGfx::eFRAMEBUFFER_NORMAL_VS] );

            shadowsFxI->setSampler( "sampl", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP ) );
            shadowsFxI->setSampler( "samplShadowMap", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_LEQUAL ) );
            shadowsFxI->setSampler( "samplNormalsVS", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP ) );

            bindCamera( ctx, camera, shadowsTexture.width, shadowsTexture.height );
            ctx->changeRenderTargets( &shadowsTexture, 1, bxGdiTexture() );
            ctx->clearBuffers( 1.f, 1.f, 1.f, 1.f, 0.f, 1, 0 );
            ctx->setViewport( bxGdiViewport( 0, 0, shadowsTexture.width, shadowsTexture.height ) );

            bxGfx::submitFullScreenQuad( ctx, shadowsFxI, "shadow" );

            ctx->changeRenderTargets( &shadowsVolumeTexture, 1, bxGdiTexture() );
            ctx->clearBuffers( 1.f, 1.f, 1.f, 1.f, 0.f, 1, 0 );
            ctx->setViewport( bxGdiViewport( 0, 0, shadowsVolumeTexture.width, shadowsVolumeTexture.height ) );
            bxGfx::submitFullScreenQuad( ctx, shadowsFxI, "shadowVolume" );
        }
    }
    ctx->clear();

    _shadows._ShowGUI();
}

void bxGfxContext::frame_drawColor( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists, bxGdiTexture ssaoTexture )
{
    bindCamera( ctx, camera, _framebuffer->width, _framebuffer->height );
    ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eALL_STAGES_MASK );

    ctx->changeRenderTargets( _framebuffer, 1, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, _framebuffer[0].width, _framebuffer[0].height ) );

    ctx->setTexture( _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS], 3, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( ssaoTexture, 4, bxGdi::eSTAGE_MASK_PIXEL );

    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR ), 3, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), 4, bxGdi::eSTAGE_MASK_PIXEL );

    for ( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeColor( _sortList_color, *rLists[ilist], camera );
    }

    _sortList_color->sortAscending();

    bxGfx::submitSortList( ctx, _cbuffer_instanceData, _sortList_color );

}

void bxGfxContext::frame_end( bxGdiContext* ctx )
{
    ctx->backend()->swap();
}

void bxGfxContext::bindCamera( bxGdiContext* ctx, const bxGfxCamera& camera, int rtWidth, int rtHeight )
{
    rtWidth = (rtWidth == -1) ? _framebuffer->width : rtWidth;
    rtHeight = (rtHeight == -1) ? _framebuffer->height : rtHeight;
    bxGfx::FrameData fdata;
    bxGfx::frameData_fill( &fdata, camera, rtWidth, rtHeight );
    ctx->backend()->updateCBuffer( _cbuffer_frameData, &fdata );
    ctx->setCbuffer( _cbuffer_frameData, 0, bxGdi::eALL_STAGES_MASK );
}
