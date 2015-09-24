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

        bxGdiTexture noiseTexture;
        bxGdiTexture imageTexture;

        bxGfxCamera camera;

        static const unsigned fbWidth = 1920;
        static const unsigned fbHeight = 1080;

        u64 timeMS;

        Data()
            : fxI( nullptr )
            , texutilFxI( nullptr )
            , timeMS( 0 )
        {}
    };
    static Data __data;
    
    inline int _LoadTextureFromFile( bxGdiTexture* tex, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* filename )
    {
        int ierr = 0;
        bxFS::File file = resourceManager->readFileSync( filename );
        if ( file.ok() )
        {
            tex[0] = dev->createTexture( file.bin, file.size );
        }
        else
        {
            ierr = -1;
        }
        file.release();

        return ierr;
    }

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
        
        _LoadTextureFromFile( &__data.noiseTexture, dev, resourceManager, "texture/noise256.dds" );
        _LoadTextureFromFile( &__data.imageTexture, dev, resourceManager, "texture/kozak.dds" );

        __data.texutilFxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );
        __data.fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tjdb" );

        __data.fxI->setTexture( "texNoise", __data.noiseTexture );
        __data.fxI->setTexture( "texImage", __data.imageTexture );
        __data.fxI->setSampler( "samplerNearest", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_WRAP ) );
        __data.fxI->setSampler( "samplerLinear", bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_WRAP ) );
        __data.fxI->setSampler( "samplerBilinear", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR, bxGdi::eADDRESS_WRAP ) );

        __data.camera.matrix.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );
    }

    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {

        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__data.fxI );
        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__data.texutilFxI );
        dev->releaseTexture( &__data.imageTexture );
        dev->releaseTexture( &__data.noiseTexture );
        dev->releaseTexture( &__data.depthRt );
        dev->releaseTexture( &__data.colorRt );
        dev->releaseVertexBuffer( &__data.screenQuad );
    }


    void tick( u64 deltaTimeMS )
    {
        __data.timeMS += deltaTimeMS;
    }


    void draw( bxGdiContext* ctx )
    {
        ctx->clear();

        ctx->changeRenderTargets( &__data.colorRt, 1, __data.depthRt );
        ctx->setViewport( bxGdiViewport( 0, 0, __data.colorRt.width, __data.colorRt.height ) );

        float clearColorRGBAD[] = { 0.f, 0.f, 0.f, 1.f, 1.f };
        ctx->clearBuffers( clearColorRGBAD, 1, 1 );
        
        const float2_t resolution( (float)__data.colorRt.width, (float)__data.colorRt.height );
        const float2_t resolutionRcp( 1.f / __data.colorRt.width, 1.f / __data.colorRt.height );

        __data.fxI->setUniform( "inResolution", resolution );
        __data.fxI->setUniform( "inResolutionRcp", resolutionRcp );
        __data.fxI->setUniform( "inTime", (float)( (double)__data.timeMS * 0.001 ) );

        
        bxGdi::shaderFx_enable( ctx, __data.fxI, "background" );
        ctx->setVertexBuffers( &__data.screenQuad, 1 );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( __data.screenQuad.numElements, 0 );
        
        //bxGdi::shaderFx_enable( ctx, __data.fxI, "foreground" );
        //ctx->draw( __data.screenQuad.numElements, 0 );

        ctx->changeToMainFramebuffer();
        ctx->clearBuffers( clearColorRGBAD, 1, 0 );
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