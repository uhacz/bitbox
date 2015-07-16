#include "gfx.h"
#include "gfx_camera.h"
#include "gfx_lights.h"
#include "gfx_type.h"

#include <gdi/gdi_context.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_render_source.h>

#include <util/float16.h>
#include <util/random.h>
#include <util/time.h>

namespace bxGfx
{
    void frameData_fill( FrameData* frameData, const bxGfxCamera& camera, int rtWidth, int rtHeight )
    {
        //SYS_STATIC_ASSERT( sizeof( FrameData ) == 376 );

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

        frameData->_renderTarget_rcp = float2_t( 1.f/ (float)rtWidth, 1.f / (float)rtHeight );
        frameData->_renderTarget_size = float2_t( (float)rtWidth, (float)rtHeight );

        //frameData->cameraParams = Vector4( fov, aspect, camera.params.zNear, camera.params.zFar );
        {
            const float m11 = proj.getElem( 0, 0 ).getAsFloat();//getCol0().getX().getAsFloat();
            const float m22 = proj.getElem( 1, 1 ).getAsFloat();//getCol1().getY().getAsFloat();
            const float m33 = proj.getElem( 2, 2 ).getAsFloat();//getCol2().getZ().getAsFloat();
            const float m44 = proj.getElem( 3, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            const float m13 = proj.getElem( 0, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();
            const float m23 = proj.getElem( 1, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            frameData->_reprojectInfo = float4_t( 1.f/m11, 1.f/m22, m33, -m44 );
            //frameData->_reprojectInfo = float4_t( 
            //    -2.f / ( (float)rtWidth*m11 ), 
            //    -2.f / ( (float)rtHeight*m22 ), 
            //    (1.f - m13) / m11, 
            //    (1.f + m23) / m22 );
            frameData->_reprojectInfoFromInt = float4_t(
                ( -frameData->_reprojectInfo.x * 2.f ) * frameData->_renderTarget_rcp.x,
                ( -frameData->_reprojectInfo.y * 2.f ) * frameData->_renderTarget_rcp.y,
                frameData->_reprojectInfo.x,
                frameData->_reprojectInfo.y
                );
            //frameData->_reprojectInfoFromInt = float4_t(
            //    frameData->_reprojectInfo.x,
            //    frameData->_reprojectInfo.y,
            //    frameData->_reprojectInfo.z + frameData->_reprojectInfo.x * 0.5f,
            //    frameData->_reprojectInfo.w + frameData->_reprojectInfo.y * 0.5f
            //    );
        }

        m128_to_xyzw( frameData->_camera_eyePos.xyzw, Vector4( camera.matrix.worldEye(), oneVec ).get128() );
        m128_to_xyzw( frameData->_camera_viewDir.xyzw, Vector4( camera.matrix.worldDir(), zeroVec ).get128() );
        
        //m128_to_xyzw( frameData->_renderTarget_rcp_size.xyzw, Vector4( 1.f / float( rtWidth ), 1.f / float( rtHeight ), float( rtWidth ), float( rtHeight ) ).get128() );
    }
}///

////
////
static bxGfx_GlobalResources* __globalResources = 0;
namespace bxGfx
{
    void globalResources_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        bxGfx_GlobalResources* gr = BX_NEW( bxDefaultAllocator(), bxGfx_GlobalResources );
        memset( gr, 0x00, sizeof( bxGfx_GlobalResources ) );
        {
            gr->fx.utils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "utils" );
            gr->fx.texUtils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );

//             gr->fx.materialRed = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "native1" );
//             gr->fx.materialGreen = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "native1" );
//             gr->fx.materialBlue = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "native1" );

        }

        {//// fullScreenQuad
            const float vertices[] =
            {
                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, -1.f, 0.f, 1.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,

                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,
                -1.f, 1.f, 0.f, 0.f, 1.f,
            };
            bxGdiVertexStreamDesc vsDesc;
            vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 );
            vsDesc.addBlock( bxGdi::eSLOT_TEXCOORD0, bxGdi::eTYPE_FLOAT, 2 );
            bxGdiVertexBuffer vBuffer = dev->createVertexBuffer( vsDesc, 6, vertices );

            gr->mesh.fullScreenQuad = bxGdi::renderSource_new( 1 );
            bxGdi::renderSource_setVertexBuffer( gr->mesh.fullScreenQuad, vBuffer, 0 );
        }
        {//// poly shapes
            bxPolyShape polyShape;
            bxPolyShape_createBox( &polyShape, 1 );
            gr->mesh.box = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );

            bxPolyShape_createShpere( &polyShape, 8 );
            gr->mesh.sphere = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );
        }

        __globalResources = gr;
    }
    void globalResources_shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        if ( !__globalResources )
            return;

        bxGfx_GlobalResources* gr = __globalResources;
        __globalResources = 0;

        {
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.sphere );
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.box );
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.fullScreenQuad );
        }
        {
//             bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.materialBlue );
//             bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.materialGreen );
//             bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.materialRed );
            
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.texUtils );
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.utils );
        }

        BX_DELETE( bxDefaultAllocator(), gr );
    }

    bxGfx_GlobalResources* globalResources()
    {
        return __globalResources;
    }


    void submitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName )
    {
        bxGfx_GlobalResources* gr = globalResources();
        bxGdi::renderSource_enable( ctx, gr->mesh.fullScreenQuad );
        bxGdi::shaderFx_enable( ctx, fxI, passName );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( gr->mesh.fullScreenQuad->vertexBuffers->numElements, 0 );

    }
    void copyTexture_RGBA( bxGdiContext* ctx, bxGdiTexture outputTexture, bxGdiTexture inputTexture )
    {
        bxGfx_GlobalResources* gr = globalResources();
        ctx->changeRenderTargets( &outputTexture, 1 );
        bxGdi::context_setViewport( ctx, outputTexture );
        submitFullScreenQuad( ctx, gr->fx.texUtils, "copy_rgba" );
    }
    void rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera )
    {
        ctx->changeToMainFramebuffer();

        bxGdiTexture colorTexture = colorFB; // _framebuffer[bxGfx::eFRAMEBUFFER_COLOR];
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();
        bxGdiViewport viewport = bxGfx::cameraParams_viewport( camera.params, backBuffer.width, backBuffer.height, colorTexture.width, colorTexture.height );

        ctx->setViewport( viewport );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );

        bxGfx_GlobalResources* gr = globalResources();
        bxGdiShaderFx_Instance* fxI = gr->fx.texUtils;
        fxI->setTexture( "gtexture", colorTexture );
        fxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );

        submitFullScreenQuad( ctx, fxI, "copy_rgba" );

    }
}///

////
////
