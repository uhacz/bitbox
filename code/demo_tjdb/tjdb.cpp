#include "tjdb.h"

#include <util/debug.h>
#include <system/window.h>
#include "resource_manager/resource_manager.h"

#include <gdi/gdi_backend.h>
#include <gdi/gdi_shader.h>
#include <gfx/gfx_camera.h>
#include "gdi/gdi_context.h"

namespace tjdb
{
    struct Data
    {
        bxGdiTexture colorRt;
        bxGdiTexture depthRt;

        bxGdiVertexBuffer screenQuad;

        bxGdiShaderFx_Instance* fxI;
        bxGdiShaderFx_Instance* texutilFxI;

        bxGfxCamera camera;

        static const unsigned fbWidth = 1920;
        static const unsigned fbHeight = 1080;

    };
    static Data __data;
    void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
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
        __data.screenQuad = dev->createVertexBuffer( vsDesc, 6, vertices );


        const int fbWidth = Data::fbWidth;
        const int fbHeight = Data::fbHeight;

        __data.colorRt = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
        __data.depthRt = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        __data.texutilFxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );

        __data.camera.matrix.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );
    }

    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__data.texutilFxI );
        dev->releaseTexture( &__data.depthRt );
        dev->releaseTexture( &__data.colorRt );
        dev->releaseVertexBuffer( &__data.screenQuad );
    }

    void draw( bxGdiContext* ctx )
    {
        ctx->changeRenderTargets( &__data.colorRt, 1, __data.depthRt );
        ctx->setViewport( bxGdiViewport( 0, 0, __data.colorRt.width, __data.colorRt.height ) );

        float clearColorRGBAD[] = { 1.f, 0.f, 0.f, 1.f, 1.f };
        ctx->clearBuffers( clearColorRGBAD, 1, 1 );

        ctx->changeToMainFramebuffer();
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();

        bxGdiViewport vp = bxGfx::cameraParams_viewport( __data.camera.params, backBuffer.width, backBuffer.height, Data::fbWidth, Data::fbHeight );
        ctx->setViewport( vp );

        __data.texutilFxI->setTexture( "gtexture", __data.colorRt );
        __data.texutilFxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );
        ctx->setVertexBuffers( &__data.screenQuad, 1 );
        bxGdi::shaderFx_enable( ctx, __data.texutilFxI, "copy_rgba" );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( __data.screenQuad.numElements, 0 );

        ctx->backend()->swap();
    }

}///