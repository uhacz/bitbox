#include "gfx.h"
#include <gdi/gdi_context.h>
#include <gdi/gdi_shader.h>
#include <util/common.h>
#include <util/range_splitter.h>

#include "gfx_camera.h"
#include "gfx_lights.h"

namespace bxGfx
{
    void frameData_fill( FrameData* frameData, const bxGfxCamera& camera, int rtWidth, int rtHeight )
    {
        SYS_STATIC_ASSERT( sizeof( FrameData ) == 352 );

        const Matrix4 sc = Matrix4::scale( Vector3(1,1,0.5f) );
        const Matrix4 tr = Matrix4::translation( Vector3(0,0,1) );
        const Matrix4 proj = sc * tr * camera.matrix.proj;

        frameData->_camera_view = camera.matrix.view;
        frameData->_camera_proj = proj;
        frameData->_camera_viewProj = proj * camera.matrix.view;
        frameData->_camera_world = camera.matrix.world;

        const float fov = camera.params.fov();
        const float aspect = camera.params.aspect();

        frameData->_camera_fov = camera.params.fov();
        frameData->_camera_aspect = camera.params.aspect();

        const float zNear = camera.params.zNear;
        const float zFar = camera.params.zFar;
        frameData->_camera_zNear = zNear;
        frameData->_camera_zFar = zFar;
        frameData->_reprojectDepthScale = (zFar - zNear) / (-zFar * zNear);
        frameData->_reprojectDepthBias = zFar / (zFar * zNear);

        //frameData->cameraParams = Vector4( fov, aspect, camera.params.zNear, camera.params.zFar );
        {
            const float a = proj.getElem( 0, 0 ).getAsFloat();//getCol0().getX().getAsFloat();
            const float b = proj.getElem( 1, 1 ).getAsFloat();//getCol1().getY().getAsFloat();
            const float c = proj.getElem( 2, 2 ).getAsFloat();//getCol2().getZ().getAsFloat();
            const float d = proj.getElem( 3, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            frameData->_camera_projParams = float4_t( 1.f/a, 1.f/b, c, -d );
        }

        m128_to_xyzw( frameData->_camera_eyePos.xyzw, Vector4( camera.matrix.worldEye(), oneVec ).get128() );
        m128_to_xyzw( frameData->_camera_viewDir.xyzw, Vector4( camera.matrix.worldDir(), zeroVec ).get128() );
        m128_to_xyzw( frameData->_renderTarget_rcp_size.xyzw, Vector4( 1.f / float( rtWidth ), 1.f / float( rtHeight ), float( rtWidth ), float( rtHeight ) ).get128() );
    }
}///

////
////
bxGfxContext::bxGfxContext()
    : _sortList_color(0)
    , _sortList_depth(0)
{}

bxGfxContext::~bxGfxContext()
{}

bxGfx::Shared bxGfxContext::_shared;
int bxGfxContext::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    const int fbWidth = 1920;
    const int fbHeight = 1080;

    _cbuffer_frameData = dev->createConstantBuffer( sizeof( bxGfx::FrameData ) );
    _cbuffer_instanceData = dev->createConstantBuffer( sizeof( bxGfx::InstanceData ) );

    _framebuffer[bxGfx::eFRAMEBUFFER_COLOR]   = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4, 0, 0 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_SWAP]    = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4, 0, 0 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH]   = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    
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

        bxPolyShape_createShpere( &polyShape, 6 );
        _shared.rsource.sphere = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
        bxPolyShape_deallocateShape( &polyShape );
    }

    const int nSortItems = 2048;
    _sortList_color  = bxGfx::sortList_new<bxGfxSortList_Color>( nSortItems, bxDefaultAllocator() );
    _sortList_depth  = bxGfx::sortList_new<bxGfxSortList_Depth>( nSortItems, bxDefaultAllocator() );
    _sortList_shadow = bxGfx::sortList_new<bxGfxSortList_Shadow>( nSortItems, bxDefaultAllocator() );

    return 0;
}

void bxGfxContext::shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    bxGfx::sortList_delete( &_sortList_depth, bxDefaultAllocator() );
    bxGfx::sortList_delete( &_sortList_color, bxDefaultAllocator() );
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

void bxGfxContext::frame_begin( bxGdiContext* ctx )
{
    ctx->clear();

    _sortList_color->clear();
    _sortList_depth->clear();
    _sortList_shadow->clear();
}

void bxGfxContext::frame_zPrepass(bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists)
{
    for ( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeDepth( _sortList_depth, *rLists[ilist], camera );
    }
    _sortList_depth->sortAscending();
    
    ctx->changeRenderTargets( 0, 0, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, _framebuffer[0].width, _framebuffer[0].height ) );
    ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eSTAGE_MASK_VERTEX );

    bxGdi::shaderFx_enable( ctx, _shared.shader.utils, "z_prepass" );
    bxGfx::submitSortList( ctx, _cbuffer_instanceData, _sortList_depth, bxGfx::eRENDER_ITEM_USE_STREAMS );
}

void bxGfxContext::frame_drawShadows( bxGdiContext* ctx, bxGfxShadows* shadows, bxGfxRenderList** rLists, int numLists, const bxGfxCamera& camera, const bxGfxLights& lights )
{
    const bxGfxLight_Sun sunLight = lights.sunLight();
    const Vector3 sunLightDirection( xyz_to_m128( sunLight.dir.xyz ) );

    float depthSplits[ bxGfx::eSHADOW_NUM_CASCADES ];
    shadows->splitDepth( depthSplits, camera.params, camera.params.zFar, 0.9f );
    shadows->computeCascades( depthSplits, camera, sunLightDirection );

    for( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeShadow( _sortList_shadow, *rLists[ilist], shadows->_cascade, bxGfx::eSHADOW_NUM_CASCADES );
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

        bxGdiTexture depthTexture = shadows->_depthTexture;
        bxGdiShaderFx_Instance* shadowsFxI = shadows->_fxI;
        bxGfxSortList_Shadow* sList = _sortList_shadow;

        ctx->changeRenderTargets( 0, 0, depthTexture );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );

        bxGdi::shaderFx_enable( ctx, shadowsFxI, "depth" );

        u8 currentCascade = 0xff;
        for( int iitem = 0; iitem < sList->_size_sortData; ++iitem )
        {
            const bxGfxSortList_Shadow::Entry& e = sList->_sortData[iitem];
            const bxGfxSortKey_Shadow key = e.key;

            SYS_ASSERT( key.cascade < bxGfx::eSHADOW_NUM_CASCADES );

            if( currentCascade != key.cascade )
            {
                currentCascade = key.cascade;
                const bxGfxShadows_Cascade& cascade = shadows->_cascade[currentCascade];

                bxGfxCamera cascadeCamera;
                cascadeCamera.matrix.view = cascade.view;
                cascadeCamera.matrix.proj = cascade.proj;
                cascadeCamera.matrix.viewProj = cascade.proj * cascade.view;
                cascadeCamera.matrix.world = inverse( cascade.view );
                cascadeCamera.params.zNear = cascade.zNear_zFar.getX().getAsFloat();
                cascadeCamera.params.zFar = cascade.zNear_zFar.getY().getAsFloat();

                bindCamera( ctx, cascadeCamera );
                ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eSTAGE_MASK_VERTEX );
                ctx->setViewport( viewports[key.cascade] );
            }

            bxGfxRenderItem_Iterator itemIt( e.rList, e.rItemIndex );
            submitRenderItem( ctx, &instanceData, _cbuffer_instanceData, itemIt, bxGfx::eRENDER_ITEM_USE_STREAMS );
        }

        {
            bindCamera( ctx, camera );
            bxGdiTexture shadowsTexture = _framebuffer[bxGfx::eFRAMEBUFFER_SHADOWS];
            

            Matrix4 viewProj[bxGfx::eSHADOW_NUM_CASCADES];
            Vector4 clipPlanes[bxGfx::eSHADOW_NUM_CASCADES];

            const Matrix4 sc = Matrix4::scale( Vector3(1,1,0.5f) );
            const Matrix4 tr = Matrix4::translation( Vector3(0,0,1) );
            for( int i = 0; i < bxGfx::eSHADOW_NUM_CASCADES; ++i )
            {
                const bxGfxShadows_Cascade& cascade = shadows->_cascade[i];
                viewProj[i] = ( sc * tr * cascade.proj ) * cascade.view;
                clipPlanes[i] = mulPerElem( cascade.zNear_zFar, Vector4(-1.f,-1.f,1.f,1.f) );
            }
            shadowsFxI->setUniform( "light_view_proj", viewProj );
            shadowsFxI->setUniform( "clip_planes", clipPlanes );
            shadowsFxI->setUniform( "light_direction_ws", sunLightDirection );
            shadowsFxI->setUniform( "occlusion_texture_size", float2_t( shadowsTexture.width, shadowsTexture.height ) );
            shadowsFxI->setUniform( "shadow_map_size", float2_t( shadows->_depthTexture.width, shadows->_depthTexture.height ) );

            shadowsFxI->setTexture( "shadowMap", shadows->_depthTexture );
            shadowsFxI->setTexture( "sceneDepthTex", _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
            shadowsFxI->setSampler( "sampl", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP ) );
            //shadowsFxI->setSampler( "samplShadowMap", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR, bxGdi::eADDRESS_CLAMP ) );
            shadowsFxI->setSampler( "samplShadowMap", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_LEQUAL ) );

            ctx->changeRenderTargets( &shadowsTexture, 1, bxGdiTexture() );
            ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );
            ctx->setViewport( bxGdiViewport( 0, 0, shadowsTexture.width, shadowsTexture.height ) );

            submitFullScreenQuad( ctx, shadowsFxI, "shadow" );
        }
    }
    ctx->clear();
}

void bxGfxContext::frame_drawColor( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists )
{
    bindCamera( ctx, camera );
    ctx->setCbuffer( _cbuffer_instanceData, 1, bxGdi::eALL_STAGES_MASK );
        
    ctx->changeRenderTargets( _framebuffer, 1, _framebuffer[bxGfx::eFRAMEBUFFER_DEPTH] );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, _framebuffer[0].width, _framebuffer[0].height ) );

    for( int ilist = 0; ilist < numLists; ++ilist )
    {
        bxGfx::sortList_computeColor( _sortList_color, *rLists[ilist], camera );
    }

    _sortList_color->sortAscending();

    bxGfx::submitSortList( ctx, _cbuffer_instanceData, _sortList_color );

}

void bxGfxContext::frame_rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera )
{
    ctx->changeToMainFramebuffer();

    bxGdiTexture colorTexture = colorFB; // _framebuffer[bxGfx::eFRAMEBUFFER_COLOR];
    bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();
    bxGdiViewport viewport = bxGfx::cameraParams_viewport( camera.params, backBuffer.width, backBuffer.height, colorTexture.width, colorTexture.height );

    ctx->setViewport( viewport );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );

    bxGdiShaderFx_Instance* fxI = _shared.shader.texUtils;
    fxI->setTexture( "gtexture", colorTexture );
    fxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );

    submitFullScreenQuad( ctx, fxI, "copy_rgba" );
}

void bxGfxContext::frame_end( bxGdiContext* ctx  )
{
    ctx->backend()->swap();
}

void bxGfxContext::submitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName )
{
    bxGdi::renderSource_enable( ctx, _shared.rsource.fullScreenQuad );
    bxGdi::shaderFx_enable( ctx, fxI, passName );
    ctx->setTopology( bxGdi::eTRIANGLES );
    ctx->draw( _shared.rsource.fullScreenQuad->vertexBuffers->numElements, 0 );
}


void bxGfxContext::bindCamera( bxGdiContext* ctx, const bxGfxCamera& camera )
{
    bxGfx::FrameData fdata;
    bxGfx::frameData_fill( &fdata, camera, _framebuffer[0].width, _framebuffer[0].height );
    ctx->backend()->updateCBuffer( _cbuffer_frameData, &fdata );

    ctx->setCbuffer( _cbuffer_frameData, 0, bxGdi::eALL_STAGES_MASK );
}


////
////
bxGfxPostprocess::bxGfxPostprocess()
    : _fxI_toneMapping(0)
{}

void bxGfxPostprocess::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    _fxI_toneMapping = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tone_mapping" );
    _fxI_fog = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "fog" );

    const int lumiTexSize = 1024;
    _toneMapping.adaptedLuminance[0] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.adaptedLuminance[1] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.initialLuminance    = dev->createTexture2D( lumiTexSize, lumiTexSize, 1 , bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );

}

void bxGfxPostprocess::_Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    dev->releaseTexture( &_toneMapping.initialLuminance );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[1] );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[0] );

    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI_fog );
    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI_toneMapping );
}

void bxGfxPostprocess::toneMapping(bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, float deltaTime)
{
    _fxI_toneMapping->setUniform( "input_size0", float2_t( (f32)inTexture.width, (f32)inTexture.height ) );
    _fxI_toneMapping->setUniform( "delta_time", deltaTime );
    _fxI_toneMapping->setUniform( "lum_tau", _toneMapping.tau );
    _fxI_toneMapping->setUniform( "auto_exposure_key_value", _toneMapping.autoExposureKeyValue );
    _fxI_toneMapping->setUniform( "camera_aperture" , _toneMapping.camera_aperture );                           
    _fxI_toneMapping->setUniform( "camera_shutterSpeed" , _toneMapping.camera_shutterSpeed ); 
    _fxI_toneMapping->setUniform( "camera_iso" , _toneMapping.camera_iso ); 
    _fxI_toneMapping->setUniform( "useAutoExposure", _toneMapping.useAutoExposure );

    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 1, bxGdi::eSTAGE_MASK_PIXEL );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.initialLuminance, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI_toneMapping, "luminance_map" );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->setTexture( _toneMapping.adaptedLuminance[!_toneMapping.currentLuminanceTexture], 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.initialLuminance, 1, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI_toneMapping, "adapt_luminance" );
    ctx->backend()->generateMipmaps( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture] );

    //
    //
    ctx->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );

    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdi::eSTAGE_MASK_PIXEL );

    bxGfxContext::submitFullScreenQuad( ctx, _fxI_toneMapping, "composite" );

    _toneMapping.currentLuminanceTexture = !_toneMapping.currentLuminanceTexture;

    ctx->setTexture( bxGdiTexture(), 0, bxGdi::eSTAGE_PIXEL );
    ctx->setTexture( bxGdiTexture(), 1, bxGdi::eSTAGE_PIXEL );

    _ShowGUI();
}

void bxGfxPostprocess::sky(bxGdiContext* ctx, bxGdiTexture outTexture, const bxGfxLight_Sun& sunLight)
{
    _fxI_fog->setUniform( "_sunDir", sunLight.dir );
    _fxI_fog->setUniform( "_sunColor", sunLight.sunColor );
    _fxI_fog->setUniform( "_skyColor", sunLight.skyColor );
    _fxI_fog->setUniform( "_sunIlluminance", sunLight.sunIlluminance );
    _fxI_fog->setUniform( "_skyIlluminance", sunLight.skyIlluminance );
    _fxI_fog->setUniform( "_fallOff", _fog.fallOff );

    ctx->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI_fog, "sky" );
}

void bxGfxPostprocess::fog( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, bxGdiTexture depthTexture, const bxGfxLight_Sun& sunLight )
{
    //_fxI_fog->setUniform( "_sunDir", sunLight.dir );
    //_fxI_fog->setUniform( "_sunColor", sunLight.color );
    //_fxI_fog->setUniform( "_sunIlluminance", sunLight.illuminance );
    //_fxI_fog->setUniform( "_skyIlluminance", 60000.f );
    //_fxI_fog->setUniform( "_fallOff", _fog.fallOff );
    
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( depthTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( inTexture, 1, bxGdi::eSTAGE_MASK_PIXEL );

    ctx->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI_fog, "fog" );

    ctx->setTexture( bxGdiTexture(), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( bxGdiTexture(), 1, bxGdi::eSTAGE_MASK_PIXEL );
}


#include "gfx_gui.h"
void bxGfxPostprocess::_ShowGUI()
{
    if ( ImGui::Begin( "gfx" ) )
    {
        if ( ImGui::TreeNode( "Postprocess" ) )
        {
            if ( ImGui::TreeNode( "ToneMapping" ) )
            {
                ImGui::InputFloat( "adaptation speed", &_toneMapping.tau );
                ///
                const char* itemName_aperture[] =
                {
                    "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11.0", "16.0",
                };
                const float itemValue_aperture[] =
                {
                    1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f,
                };
                static int currentItem_aperture = 7;
                if ( ImGui::Combo( "aperture", &currentItem_aperture, itemName_aperture, sizeof( itemValue_aperture ) / sizeof( *itemValue_aperture ) ) )
                {
                    _toneMapping.camera_aperture = itemValue_aperture[currentItem_aperture];
                }

                ///
                const char* itemName_shutterSpeed[] =
                {
                    "1/2000", "1/1000", "1/500", "1/250", "1/125", "1/100", "1/60", "1/30", "1/15", "1/8", "1/4", "1/2", "1/1",
                };
                const float itemValue_shutterSpeed[] =
                {
                    1.f / 2000.f, 1.f / 1000.f, 1.f / 500.f, 1.f / 250.f, 1.f / 125.f, 1.f / 100.f, 1.f / 60.f, 1.f / 30.f, 1.f / 15.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f, 1.f / 1.f,
                };
                static int currentItem_shutterSpeed = 5;
                if ( ImGui::Combo( "shutter speed", &currentItem_shutterSpeed, itemName_shutterSpeed, sizeof( itemValue_shutterSpeed ) / sizeof( *itemValue_shutterSpeed ) ) )
                {
                    _toneMapping.camera_shutterSpeed = itemValue_shutterSpeed[currentItem_shutterSpeed];
                }

                ///
                const char* itemName_iso[] =
                {
                    "100", "200", "400", "800", "1600", "3200", "6400",
                };
                const float itemValue_iso[] =
                {
                    100.f, 200.f, 400.f, 800.f, 1600.f, 3200.f, 6400.f,
                };
                static int currentItem_iso = 0;
                if ( ImGui::Combo( "ISO", &currentItem_iso, itemName_iso, sizeof( itemValue_iso ) / sizeof( *itemValue_iso ) ) )
                {
                    _toneMapping.camera_iso = itemValue_iso[currentItem_iso];
                }

                ImGui::Checkbox( "useAutoExposure", (bool*)&_toneMapping.useAutoExposure );

                ImGui::TreePop();
            }

            if ( ImGui::TreeNode( "Fog" ) )
            {
                ImGui::SliderFloat( "fallOff", &_fog.fallOff, 0.f, 1.f, "%.8", 4.f );
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
        ImGui::End();
    }

}

