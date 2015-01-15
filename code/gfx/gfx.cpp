#include "gfx.h"
#include <gdi/gdi_context.h>
#include <gdi/gdi_shader.h>
#include <util/common.h>
#include <util/range_splitter.h>

#include "gfx_camera.h"
#include "gfx_lights.h"

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
        frameData->renderTargetSizeRcp = Vector4( 1.f / float( rtWidth ), 1.f / float( rtHeight ), float( rtWidth ), float( rtHeight ) );
    }
}///

////
////
bxGfxContext::bxGfxContext()
{}

bxGfxContext::~bxGfxContext()
{}

bxGfx::Shared bxGfxContext::_shared;
int bxGfxContext::startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    const int fbWidth = 1920;
    const int fbHeight = 1080;

    _cbuffer_frameData = dev->createConstantBuffer( sizeof( bxGfx::FrameData ) );
    _cbuffer_instanceData = dev->createConstantBuffer( sizeof( bxGfx::InstanceData ) );

    _framebuffer[bxGfx::eFRAMEBUFFER_COLOR] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4, 0, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
    _framebuffer[bxGfx::eFRAMEBUFFER_SWAP]  = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4, 0, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
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

        bxPolyShape_createShpere( &polyShape, 6 );
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

        bxRangeSplitter split = bxRangeSplitter::splitByGrab( nInstances, bxGfx::cMAX_WORLD_MATRICES );
        while( split.elementsLeft() )
        {
            const int offset = split.grabbedElements;
            const int grab = split.nextGrab();

            for ( int imatrix = 0; imatrix < grab; ++imatrix )
            {
                bxGfx::instanceData_setMatrix( &instanceData, imatrix, worldMatrices[offset + imatrix] );
            }

            ctx->backend()->updateCBuffer( _cbuffer_instanceData, &instanceData );

            if ( ctx->indicesBound() )
            {
                for ( int isurface = 0; isurface < nSurfaces; ++isurface )
                {
                    const bxGdiRenderSurface& surf = surfaces[isurface];
                    bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, grab );
                }
            }
            else
            {
                for ( int isurface = 0; isurface < nSurfaces; ++isurface )
                {
                    const bxGdiRenderSurface& surf = surfaces[isurface];
                    bxGdi::renderSurface_drawInstanced( ctx, surf, grab );
                }
            }
        }

        itemIt.next();
    }

    
}

void bxGfxContext::rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera )
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

    //bxGdi::renderSource_enable( ctx, _shared.rsource.fullScreenQuad );
    //bxGdi::shaderFx_enable( ctx, fxI, "copy_rgba" );
    //ctx->setTopology( bxGdi::eTRIANGLES );
    //ctx->draw( _shared.rsource.fullScreenQuad->vertexBuffers->numElements, 0 );
}

void bxGfxContext::frameEnd( bxGdiContext* ctx  )
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

////
////
bxGfxPostprocess::bxGfxPostprocess()
    : _fxI(0)
{}

void bxGfxPostprocess::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    _fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tone_mapping" );

    _toneMapping.adaptedLuminance[0] = dev->createTexture2D( 1024, 1024, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.adaptedLuminance[1] = dev->createTexture2D( 1024, 1024, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.initialLuminance = dev->createTexture2D( 1024, 1024, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );

}

void bxGfxPostprocess::_Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    dev->releaseTexture( &_toneMapping.initialLuminance );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[1] );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[0] );

    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI );
}

void bxGfxPostprocess::toneMapping(bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, int fbWidth, int fbHeight, float deltaTime)
{
    _fxI->setUniform( "input_size0", float2_t( (f32)fbWidth, (f32)fbHeight ) );
    _fxI->setUniform( "delta_time", deltaTime );
    _fxI->setUniform( "lum_tau", _toneMapping.tau );
    _fxI->setUniform( "auto_exposure_key_value", _toneMapping.autoExposureKeyValue );
    _fxI->setUniform( "camera_aperture" , _toneMapping.camera_aperture );                           
    _fxI->setUniform( "camera_shutterSpeed" , _toneMapping.camera_shutterSpeed ); 
    _fxI->setUniform( "camera_iso" , _toneMapping.camera_iso ); 

    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 1, bxGdi::eSTAGE_MASK_PIXEL );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.initialLuminance, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI, "luminance_map" );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->setTexture( _toneMapping.adaptedLuminance[!_toneMapping.currentLuminanceTexture], 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.initialLuminance, 1, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfxContext::submitFullScreenQuad( ctx, _fxI, "adapt_luminance" );
    ctx->backend()->generateMipmaps( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture] );

    //
    //
    ctx->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, fbWidth, fbHeight ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );

    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdi::eSTAGE_MASK_PIXEL );

    bxGfxContext::submitFullScreenQuad( ctx, _fxI, "composite" );

    _toneMapping.currentLuminanceTexture = !_toneMapping.currentLuminanceTexture;

    _ShowGUI();
}

#include "gfx_gui.h"
void bxGfxPostprocess::_ShowGUI()
{
    if( ImGui::Begin( "Postprocess" ) )
    {
        if( ImGui::TreeNode( "ToneMapping" ) )
        {
            ///
            const char* itemName_aperture[] = 
            {
                "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11.0", "16.0",
            };
            const float itemValue_aperture[] = 
            {
                1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f,
            };
            static int currentItem_aperture = 0;
            if( ImGui::Combo( "aperture", &currentItem_aperture, itemName_aperture, sizeof( itemValue_aperture ) / sizeof(*itemValue_aperture) )  )
            {
                _toneMapping.camera_aperture = itemValue_aperture[ currentItem_aperture ];
            }
            
            ///
            const char* itemName_shutterSpeed[] = 
            { 
                "1/1000", "1/500", "1/250", "1/125", "1/60", "1/30", "1/15", "1/8", "1/4", "1/2", "1/1",
            };
            const float itemValue_shutterSpeed[] = 
            {
                1.f/1000.f, 1.f/500.f, 1.f/250.f, 1.f/125.f, 1.f/60.f, 1.f/30.f, 1.f/15.f, 1.f/8.f, 1.f/4.f, 1.f/2.f, 1.f/1.f,
            };
            static int currentItem_shutterSpeed = 0;
            if( ImGui::Combo( "shutter speed", &currentItem_shutterSpeed, itemName_shutterSpeed, sizeof( itemValue_shutterSpeed ) / sizeof(*itemValue_shutterSpeed) )  )
            {
                _toneMapping.camera_shutterSpeed = itemValue_shutterSpeed[ currentItem_shutterSpeed ];
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
            if( ImGui::Combo( "ISO", &currentItem_iso, itemName_iso, sizeof( itemValue_iso ) / sizeof(*itemValue_iso) )  )
            {
                _toneMapping.camera_iso = itemValue_iso[ currentItem_iso ];
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }
}

