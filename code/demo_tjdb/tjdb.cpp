#include "tjdb.h"

#include <util/debug.h>
#include <util/signal_filter.h>
#include <util/array.h>
#include <util/common.h>
#include <system/window.h>
#include <resource_manager/resource_manager.h>

#include <gdi/gdi_backend.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>
#include <gfx/gfx_camera.h>

#include <bass/bass.h>

namespace tjdb
{
    Metronome::Metronome()
        : clickUS_( 0 )
        , timerUS_( 0 )
        , clickTimerUS_( 0 )
        , flag_click_( 0 )
    {}

    void Metronome::init( int bpm, float meter )
    {
        clickUS_ = (u64)( ( ( 60.0 ) / double( bpm ) ) * 1000000.0 * ( 1.0 / (double)meter ) );
        timerUS_ = 0;
        clickTimerUS_ = 0;
        flag_click_ = 0;
    }

    void Metronome::update( u64 deltaTimeUS )
    {
        timerUS_ = deltaTimeUS;
        clickTimerUS_ += deltaTimeUS;

        if( clickTimerUS_ >= clickUS_ )
        {
            while( clickTimerUS_ >= clickUS_ )
            {
                clickTimerUS_ -= clickUS_;
            }

            flag_click_ = 1;
        }
        else
        {
            flag_click_ = 0;
        }
    }

    static const unsigned FB_WIDTH = 1920;
    static const unsigned FB_HEIGHT = 1080;
    static const unsigned FFT_BINS = 256;
    static const float FFT_LOWPASS_RC = 0.025f;

    struct Data
    {
        bxGdiTexture colorBg;
        bxGdiTexture colorFg;
        //bxGdiTexture depthRt;

        bxGdiVertexBuffer screenQuad;

        bxGdiShaderFx_Instance* fxI;
        bxGdiShaderFx_Instance* texutilFxI;

        bxGdiTexture noiseTexture;
        bxGdiTexture imageTexture;
        bxGdiTexture logoTexture;
        bxGdiTexture txtTexture;
        bxGdiTexture maskHiTexture;
        bxGdiTexture maskLoTexture;
        bxGdiTexture fftTexture;

        bxGfxCamera camera;

        HSTREAM soundStream;
        bxFS::File soundFile;

        f32 fftDataPrev[FFT_BINS];
        f32 fftDataCurr[FFT_BINS];

        array_t< float2_t > targetPoint;
        i32 currentTargetPointIndex;
        f32 currentTargetZoom;

        float2_t currentPoint;
        f32 currentZoom;
        f32 zoomSpeed;
        f32 targetSpeed;

        f32 fadeValueInv;
        u64 timeMS;

        u32 flag_stopRequest : 1;

        Data()
            : fxI( nullptr )
            , texutilFxI( nullptr )
            , soundStream( 0 )
            , currentTargetPointIndex(-1)
            , currentTargetZoom(1.f)
            , currentPoint( 0.5f, 0.5f )
            , currentZoom( 1.f )
            , zoomSpeed( 1.f )
            , targetSpeed( 1.f )
            , fadeValueInv( 0.f )
            , timeMS( 0 )
            , flag_stopRequest( 0 )
        {
            memset( fftDataPrev, 0x00, sizeof( fftDataPrev ) );
            memset( fftDataCurr, 0x00, sizeof( fftDataCurr ) );
        }
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

    void startup( bxWindow* win, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
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


        const int fbWidth = FB_WIDTH;
        const int fbHeight = FB_HEIGHT;
        __data.colorBg = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
        __data.colorFg = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );
        //__data.depthRt = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
        
        _LoadTextureFromFile( &__data.noiseTexture, dev, resourceManager, "texture/noise256.dds" );
        _LoadTextureFromFile( &__data.imageTexture, dev, resourceManager, "texture/kozak.dds" );
        _LoadTextureFromFile( &__data.logoTexture, dev, resourceManager, "texture/logo.dds" );
        _LoadTextureFromFile( &__data.txtTexture, dev, resourceManager, "texture/txt.dds" );
        _LoadTextureFromFile( &__data.maskHiTexture, dev, resourceManager, "texture/kozak_maskHi.dds" );
        _LoadTextureFromFile( &__data.maskLoTexture, dev, resourceManager, "texture/kozak_maskLo.dds" );
        __data.fftTexture = dev->createTexture1D( FFT_BINS, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_SHADER_RESOURCE, 0, NULL );

        __data.texutilFxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );
        __data.fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tjdb" );

        __data.fxI->setTexture( "texNoise", __data.noiseTexture );
        __data.fxI->setTexture( "texImage", __data.imageTexture );
        __data.fxI->setTexture( "texLogo", __data.logoTexture );
        __data.fxI->setTexture( "texText", __data.txtTexture );
        __data.fxI->setTexture( "texMaskHi", __data.maskHiTexture );
        __data.fxI->setTexture( "texMaskLo", __data.maskLoTexture );
        __data.fxI->setTexture( "texFFT", __data.fftTexture );
        __data.fxI->setTexture( "texBackground", __data.colorBg );
        __data.fxI->setSampler( "samplerNearest", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_WRAP ) );
        __data.fxI->setSampler( "samplerLinear", bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_WRAP ) );
        __data.fxI->setSampler( "samplerBilinear", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR, bxGdi::eADDRESS_WRAP ) );
        __data.fxI->setSampler( "samplerBilinearBorder", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR, bxGdi::eADDRESS_BORDER ) );

        __data.camera.matrix.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );
        
        if( !BASS_Init( -1, 44100, 0, win->hwnd, NULL ) )
        {
            bxLogError( "Sound init failed" );
        }
        else
        {
            const char* musicFilename = "sound/TJDB3.wav";
            bxFS::File file = resourceManager->readFileSync( musicFilename );
            if ( file.ok() )
            {
                __data.soundStream = BASS_StreamCreateFile( true, file.bin, 0, file.size, 0 );
                __data.soundFile = file;
            }
        }

        {
            array::push_back( __data.targetPoint, float2_t( 0.5f, 0.5f ) );
            array::push_back( __data.targetPoint, float2_t( 0.1f, 0.1f ) );
            array::push_back( __data.targetPoint, float2_t( 0.9f, 0.9f ) );
            array::push_back( __data.targetPoint, float2_t( 0.1f, 0.9f ) );
            array::push_back( __data.targetPoint, float2_t( 0.9f, 0.1f ) );

            __data.currentTargetPointIndex = 1;
            __data.currentPoint = __data.targetPoint[1];
            __data.currentZoom = 1.0f;
            __data.currentTargetZoom = 1.5f;
            __data.zoomSpeed = 1.f / 2.f;
            __data.targetSpeed = 1.f;

        }


    }

    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        BASS_StreamFree( __data.soundStream );
        __data.soundFile.release();
        BASS_Free();

        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__data.fxI );
        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__data.texutilFxI );
        dev->releaseTexture( &__data.fftTexture );
        dev->releaseTexture( &__data.maskLoTexture );
        dev->releaseTexture( &__data.maskHiTexture );
        dev->releaseTexture( &__data.logoTexture );
        dev->releaseTexture( &__data.txtTexture );
        dev->releaseTexture( &__data.imageTexture );
        dev->releaseTexture( &__data.noiseTexture );
        //dev->releaseTexture( &__data.depthRt );
        dev->releaseTexture( &__data.colorFg );
        dev->releaseTexture( &__data.colorBg );
        dev->releaseVertexBuffer( &__data.screenQuad );
    }


    void tick( const bxInput* input, bxGdiContext* ctx, u64 deltaTimeMS )
    {
        if( bxInput_isKeyPressedOnce( &input->kbd, '1' ) )
        {
            if( BASS_ChannelIsActive( __data.soundStream ) )
            {
                BASS_ChannelStop( __data.soundStream );
            }
            else
            {
                BASS_ChannelPlay( __data.soundStream, false );
            }
        }
        else if( bxInput_isKeyPressedOnce( &input->kbd, '2' ) )
        {
            __data.flag_stopRequest = 1;
            BASS_ChannelSlideAttribute( __data.soundStream, BASS_ATTRIB_VOL, 0.f, 2000 );
        }

        const float deltaTime = (float)((double)deltaTimeMS * 0.001);
        if( BASS_ChannelIsActive( __data.soundStream ) )
        {
            memcpy( __data.fftDataPrev, __data.fftDataCurr, FFT_BINS * sizeof( f32 ) );
            int ierr = BASS_ChannelGetData( __data.soundStream, __data.fftDataCurr, BASS_DATA_FFT512 );
            if( ierr != -1 )
            {
                for( int i = 0; i < FFT_BINS; ++i )
                {
                    float sampleValue = ::sqrt( __data.fftDataCurr[i] );
                    __data.fftDataCurr[i] = signalFilter_lowPass( sampleValue, __data.fftDataPrev[i], FFT_LOWPASS_RC, deltaTime );
                }

                ctx->backend()->updateTexture( __data.fftTexture, __data.fftDataCurr );
            }
        }



        if ( BASS_ChannelIsActive( __data.soundStream ) )
        {
            __data.timeMS += deltaTimeMS;

            if( __data.flag_stopRequest )
            {
                if( __data.fadeValueInv <= 0.f )
                {
                    BASS_ChannelStop( __data.soundStream );
                    BASS_ChannelSetPosition( __data.soundStream, 0, BASS_POS_BYTE );
                    BASS_ChannelSetAttribute( __data.soundStream, BASS_ATTRIB_VOL, 1.f );
                    __data.timeMS = 0;
                    __data.flag_stopRequest = 0;
                }

                __data.fadeValueInv -= deltaTime * 0.5f;
            }
            else
            {
                __data.fadeValueInv += deltaTime * 0.5f;
            }

            __data.fadeValueInv = clamp( __data.fadeValueInv, 0.f, 1.f );
        }
    }


    void draw( bxGdiContext* ctx )
    {
        ctx->clear();

        ctx->changeRenderTargets( &__data.colorBg, 1 );
        ctx->setViewport( bxGdiViewport( 0, 0, __data.colorBg.width, __data.colorBg.height ) );

        float clearColorRGBAD[] = { 0.f, 0.f, 0.f, 1.f, 1.f };
        ctx->clearBuffers( clearColorRGBAD, 1, 1 );
        
        const float2_t resolution( (float)__data.colorFg.width, (float)__data.colorFg.height );
        const float2_t resolutionRcp( 1.f / __data.colorFg.width, 1.f / __data.colorFg.height );

        __data.fxI->setUniform( "inResolution", resolution );
        __data.fxI->setUniform( "inResolutionRcp", resolutionRcp );
        __data.fxI->setUniform( "inTime", (float)( (double)__data.timeMS * 0.001 ) );
        __data.fxI->setUniform( "fadeValueInv", __data.fadeValueInv );

        bxGdi::shaderFx_enable( ctx, __data.fxI, "background" );
        ctx->setVertexBuffers( &__data.screenQuad, 1 );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( __data.screenQuad.numElements, 0 );

//         bxGdi::shaderFx_enable( ctx, __data.fxI, "text" );
//         ctx->setVertexBuffers( &__data.screenQuad, 1 );
//         ctx->setTopology( bxGdi::eTRIANGLES );
//         ctx->draw( __data.screenQuad.numElements, 0 );
        
        ctx->changeRenderTargets( &__data.colorFg, 1 );
        ctx->setViewport( bxGdiViewport( 0, 0, __data.colorFg.width, __data.colorFg.height ) );
        bxGdi::shaderFx_enable( ctx, __data.fxI, "foreground" );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( __data.screenQuad.numElements, 0 );

        //bxGdi::shaderFx_enable( ctx, __data.fxI, "foreground" );
        //ctx->draw( __data.screenQuad.numElements, 0 );

        ctx->changeToMainFramebuffer();
        ctx->clearBuffers( clearColorRGBAD, 1, 0 );
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();

        bxGdiViewport vp = bxGfx::cameraParams_viewport( __data.camera.params, backBuffer.width, backBuffer.height, FB_WIDTH, FB_HEIGHT );
        ctx->setViewport( vp );

        __data.texutilFxI->setTexture( "gtexture", __data.colorFg );
        __data.texutilFxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );
        ctx->setVertexBuffers( &__data.screenQuad, 1 );
        bxGdi::shaderFx_enable( ctx, __data.texutilFxI, "copy_rgba" );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( __data.screenQuad.numElements, 0 );

        ctx->backend()->swap();
    }

}///