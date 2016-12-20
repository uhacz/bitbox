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
#include "gfx/gui/imgui/imgui.h"

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

        f32 fadeValueInv;
        u64 timeMS;
        f32 jumpToTimeValueS;


        u32 flag_stopRequest : 1;
        u32 flag_jumpToTime : 1;

        Data()
            : fxI( nullptr )
            , texutilFxI( nullptr )
            , soundStream( 0 )
            , fadeValueInv( 0.f )
            , timeMS( 0 )
            , jumpToTimeValueS( 0.f )
            , flag_stopRequest( 0 )
            , flag_jumpToTime( 0 )
        {
            memset( fftDataPrev, 0x00, FFT_BINS * sizeof( *fftDataPrev ) );
            memset( fftDataCurr, 0x00, FFT_BINS * sizeof( *fftDataCurr ) );
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
        __data.fxI->setSampler( "samplerTrilinear", bxGdiSamplerDesc( bxGdi::eFILTER_TRILINEAR, bxGdi::eADDRESS_BORDER ) );

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
    }

    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        BASS_StreamFree( __data.soundStream );
        BASS_Free();
        __data.soundFile.release();

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
            else if ( __data.flag_jumpToTime )
            {
                __data.flag_jumpToTime = 0;
                QWORD jumpToTimeInBytes = BASS_ChannelSeconds2Bytes( __data.soundStream, __data.jumpToTimeValueS );
                BASS_ChannelSetPosition( __data.soundStream, jumpToTimeInBytes, BASS_POS_BYTE );
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

        QWORD musicLengthInBytes = BASS_ChannelGetLength( __data.soundStream, BASS_POS_BYTE );
        QWORD musicPosInBytes = BASS_ChannelGetPosition( __data.soundStream, BASS_POS_BYTE );

        float musicLengthInSec = (float)BASS_ChannelBytes2Seconds( __data.soundStream, musicLengthInBytes );
        float musicPosInSec = (float)BASS_ChannelBytes2Seconds( __data.soundStream, musicPosInBytes );

        __data.fxI->setUniform( "inResolution", resolution );
        __data.fxI->setUniform( "inResolutionRcp", resolutionRcp );
        //__data.fxI->setUniform( "inTime", (float)( (double)__data.timeMS * 0.001 ) );
        __data.fxI->setUniform( "inTime", musicPosInSec );
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

        //if( ImGui::Begin( "system" ) )
        //{
        //    if( ImGui::SliderFloat( "timeline", &musicPosInSec, 0.f, musicLengthInSec ) )
        //    {
        //        __data.flag_jumpToTime = 1;
        //        __data.jumpToTimeValueS = musicPosInSec;
        //    }
        //}
        //ImGui::End();

    }

    App::App() : _resourceManager( nullptr )
        , _gdiDev( nullptr )
        , _gdiCtx( nullptr )
    {

    }

    bool App::startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bxConfig::global_init();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        _resourceManager = bxResourceManager::startup( assetDir );

        bxGdi::backendStartup( &_gdiDev, (uptr)win->hwnd, win->width, win->height, win->full_screen );
        _gdiCtx = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiCtx->_Startup( _gdiDev );

        tjdb::startup( win, _gdiDev, _resourceManager );

        bxGfxGUI::_Startup( _gdiDev, _resourceManager, win );

        return true;
    }

    void App::shutdown()
    {
        bxWindow* win = bxWindow_get();
        bxGfxGUI::shutdown( _gdiDev, _resourceManager, win );

        tjdb::shutdown( _gdiDev, _resourceManager );
        _gdiCtx->_Shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gdiCtx );
        bxGdi::backendShutdown( &_gdiDev );
        bxResourceManager::shutdown( &_resourceManager );
        bxConfig::global_deinit();
    }

    bool App::update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const u64 deltaTimeMS = deltaTimeUS / 1000;

        bxGfxGUI::newFrame( deltaTimeMS * 0.001f );

        tjdb::tick( &win->input, _gdiCtx, deltaTimeMS );

        tjdb::draw( _gdiCtx );

        bxGfxGUI::draw( _gdiCtx );

        _gdiCtx->backend()->swap();

        return true;
    }

}///