#include "no_gods_no_masters.h"
#include <system\window.h>
#include <util/config.h>
#include <util/signal_filter.h>
#include <math.h>

namespace tjdb
{
    NoGodsNoMasters gTJDB = {};
    //////////////////////////////////////////////////////////////////////////
    static void ChangeTargetAndClear( rdi::CommandQueue* cmdq, rdi::TextureRW texture, const float clearColor[5] )
    {
        rdi::context::ChangeRenderTargets( cmdq, &texture, 1, rdi::TextureDepth() );
        rdi::context::ClearBuffers( cmdq, &texture, 1, rdi::TextureDepth(), clearColor, 1, 0 );
    }
    void PostProcessPass::_StartUp( PostProcessPass* pass )
    {
        // --- ToneMapping
        {
            PostProcessPass::ToneMapping& tm = pass->_tm;
            PostProcessPass::ToneMapping::MaterialData& data = tm.data;
            data.input_size0 = float2_t( 0.f, 0.f );
            data.delta_time = 1.f / 60.f;

            data.bloom_thresh = 0.f;
            data.bloom_blur_sigma = 0.f;
            data.bloom_magnitude = 0.f;

            data.lum_tau = 5.f;
            //data.adaptation_rate = 0.5f;
            data.exposure_key_value = 0.30f;
            data.use_auto_exposure = 0;
            data.camera_aperture = 1.f / 16.f;
            data.camera_shutterSpeed = 0.005f;
            data.camera_iso = 200.f;

            tm.cbuffer_data = rdi::device::CreateConstantBuffer( sizeof( PostProcessPass::ToneMapping::MaterialData ), &data );

            const int lumiTexSize = 1024;
            tm.adapted_luminance[0] = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 11, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );
            tm.adapted_luminance[1] = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 11, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );
            tm.initial_luminance = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 1, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );

            rdi::ShaderFile* sf = rdi::ShaderFileLoad( "shader/bin/tone_mapping.shader", GResourceManager() );

            rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
            rdi::PipelineDesc pipeline_desc = {};

            {
                pipeline_desc.Shader( sf, "luminance_map" );
                tm.pipeline_luminance_map = rdi::CreatePipeline( pipeline_desc );
            }

            {
                pipeline_desc.Shader( sf, "adapt_luminance" );
                tm.pipeline_adapt_luminance = rdi::CreatePipeline( pipeline_desc );
                rdesc = rdi::GetResourceDescriptor( tm.pipeline_adapt_luminance );
                rdi::SetConstantBuffer( rdesc, "MaterialData", &tm.cbuffer_data );
            }

            {
                pipeline_desc.Shader( sf, "composite" );
                tm.pipeline_composite = rdi::CreatePipeline( pipeline_desc );
                rdesc = rdi::GetResourceDescriptor( tm.pipeline_composite );
                rdi::SetConstantBuffer( rdesc, "MaterialData", &tm.cbuffer_data );
            }
        }
    }

    void PostProcessPass::_ShutDown( PostProcessPass* pass )
    {
        // --- ToneMapping
        {
            PostProcessPass::ToneMapping& tm = pass->_tm;
            rdi::DestroyPipeline( &tm.pipeline_composite );
            rdi::DestroyPipeline( &tm.pipeline_adapt_luminance );
            rdi::DestroyPipeline( &tm.pipeline_luminance_map );

            rdi::device::DestroyTexture( &tm.initial_luminance );
            rdi::device::DestroyTexture( &tm.adapted_luminance[1] );
            rdi::device::DestroyTexture( &tm.adapted_luminance[0] );

            rdi::device::DestroyConstantBuffer( &tm.cbuffer_data );
        }
    }

    void PostProcessPass::DoToneMapping( rdi::CommandQueue* cmdq, rdi::TextureRW outTexture, rdi::TextureRW inTexture, float deltaTime )
    {
        // --- ToneMapping
        {
            //SYS_STATIC_ASSERT( sizeof( PostProcessPass::ToneMapping::MaterialData ) == 44 );
            _tm.data.input_size0 = float2_t( (float)outTexture.info.width, (float)outTexture.info.height );
            _tm.data.delta_time = deltaTime;
            //_tm.data.adaptation_rate = adaptationRate;
            rdi::context::UpdateCBuffer( cmdq, _tm.cbuffer_data, &_tm.data );


            const float clear_color[5] = { 0.f, 0.f, 0.f, 0.f, 1.0f };

            { // --- LuminanceMap
                ChangeTargetAndClear( cmdq, _tm.initial_luminance, clear_color );

                rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_luminance_map );
                rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
                rdi::BindPipeline( cmdq, _tm.pipeline_luminance_map, true );
                gTJDB.DrawFullScreenQuad( cmdq );
            }

            { // --- AdaptLuminance
                ChangeTargetAndClear( cmdq, _tm.CurrLuminanceTexture(), clear_color );
                rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_adapt_luminance );
                rdi::SetResourceRO( rdesc, "tex_input0", &_tm.PrevLuminanceTexture() );
                rdi::SetResourceRO( rdesc, "tex_input1", &_tm.initial_luminance );
                rdi::BindPipeline( cmdq, _tm.pipeline_adapt_luminance, true );
                gTJDB.DrawFullScreenQuad( cmdq );

                rdi::context::GenerateMipmaps( cmdq, _tm.CurrLuminanceTexture() );
            }

            { // --- Composite
                ChangeTargetAndClear( cmdq, outTexture, clear_color );
                rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_composite );
                rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
                rdi::SetResourceRO( rdesc, "tex_input1", &_tm.CurrLuminanceTexture() );
                rdi::BindPipeline( cmdq, _tm.pipeline_composite, true );
                gTJDB.DrawFullScreenQuad( cmdq );
            }

            _tm.current_luminance_texture = !_tm.current_luminance_texture;
        }
    }
}

namespace tjdb
{
    static const float FFT_LOWPASS_RC = 0.025f;

    rdi::TextureRO LoadTextureFromFile( const char* filename )
    {
        rdi::TextureRO texture = {};

        bxFS::File file = GResourceManager()->readFileSync( filename );
        if( file.ok() )
        {
            texture = rdi::device::CreateTextureFromDDS( file.bin, file.size );
        }
        file.release();

        return texture;
    }


    bool NoGodsNoMasters::startup( int argc, const char** argv )
    {
        const char* cfg_filename = "demo_tjdb/global.cfg";
        bxConfig::global_init( cfg_filename );

        bxWindow* win = bxWindow_get();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        ResourceManager::startup( assetDir );
        
        rdi::Startup( (uptr)win->hwnd, win->width, win->height, win->full_screen );
        
        // ---
        _fullscreen_quad = rdi::CreateFullscreenQuad();
        _bg_texture = LoadTextureFromFile( "texture/tjdb/okladka_bg.DDS" );
        _bg_mask = LoadTextureFromFile( "texture/tjdb/maska_bg.DDS" );
        _spis_texture = LoadTextureFromFile( "texture/tjdb/maska_spis.DDS" );
        _fft_texture = rdi::device::CreateTexture1D( FFT_BINS, 1, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::SHADER_RESOURCE, 0, NULL );

        const u32 w = _bg_texture.info.width;
        const u32 h = _bg_texture.info.height;
        _color_texture = rdi::device::CreateTexture2D( w, h, 1, rdi::Format( rdi::EDataType::FLOAT, 4 ), rdi::EBindMask::SHADER_RESOURCE | rdi::EBindMask::RENDER_TARGET, 0, nullptr );
        _swap_texture = rdi::device::CreateTexture2D( w, h, 1, rdi::Format( rdi::EDataType::FLOAT, 4 ), rdi::EBindMask::SHADER_RESOURCE | rdi::EBindMask::RENDER_TARGET, 0, nullptr );

        PostProcessPass::_StartUp( &_post_process );

        rdi::ShaderFile* shf = rdi::ShaderFileLoad( "shader/bin/tjdb_no_gods_no_masters.shader", GResourceManager() );

        rdi::PipelineDesc pipeline_desc = {};
        pipeline_desc.Shader( shf, "blit" );
        _pipeline_blit = rdi::CreatePipeline( pipeline_desc );

        pipeline_desc.Shader( shf, "main" );
        _pipeline_main = rdi::CreatePipeline( pipeline_desc );

        rdi::ShaderFileUnload( &shf, GResourceManager() );

        rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_blit );
        

        {
            rdi::SamplerDesc samp_desc = {};

            samp_desc.Filter( rdi::ESamplerFilter::NEAREST );
            _samplers.point = rdi::device::CreateSampler( samp_desc );

            samp_desc.Filter( rdi::ESamplerFilter::LINEAR );
            _samplers.linear = rdi::device::CreateSampler( samp_desc );

            samp_desc.Filter( rdi::ESamplerFilter::BILINEAR_ANISO );
            _samplers.bilinear = rdi::device::CreateSampler( samp_desc );

            samp_desc.Filter( rdi::ESamplerFilter::TRILINEAR_ANISO );
            _samplers.trilinear = rdi::device::CreateSampler( samp_desc );

            rdi::ResourceBinding samp_bindings[] =
            {
                rdi::ResourceBinding( "point", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 0 ),
                rdi::ResourceBinding( "linear", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 1 ),
                rdi::ResourceBinding( "bilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 2 ),
                rdi::ResourceBinding( "trilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 3 ),
            };
            rdi::ResourceLayout resource_layout = {};
            resource_layout.bindings = samp_bindings;
            resource_layout.num_bindings = sizeof( samp_bindings ) / sizeof( *samp_bindings );
            _samplers.resource_desc = rdi::CreateResourceDescriptor( resource_layout );

            rdi::SetSampler( _samplers.resource_desc, "point", &_samplers.point );
            rdi::SetSampler( _samplers.resource_desc, "linear", &_samplers.linear );
            rdi::SetSampler( _samplers.resource_desc, "bilinear_aniso", &_samplers.bilinear );
            rdi::SetSampler( _samplers.resource_desc, "trilinear_aniso", &_samplers.trilinear );
        }


        if( !BASS_Init( -1, 44100, 0, win->hwnd, NULL ) )
        {
            bxLogError( "Sound init failed" );
            SYS_NOT_IMPLEMENTED;
        }

        for( u32 i = 0; i < NUM_SONGS; ++i )
        {
            char musicFilename[64];
            sprintf_s( musicFilename, 64, "sound/tjdb/%u.wav", i+1 );
            bxFS::File file = bx::GResourceManager()->readFileSync( musicFilename );
            SYS_ASSERT( file.ok() );
            _song_files[i] = file;
            _song_streams[i] = BASS_StreamCreateFile( true, file.bin, 0, file.size, 0 );
        }

        return true;
    }

    void NoGodsNoMasters::shutdown()
    {
        for( u32 i = 0; i < NUM_SONGS; ++i )
        {
            BASS_StreamFree( _song_streams[i] );
            _song_files[i].release();
        }
        BASS_Free();

        {
            rdi::DestroyResourceDescriptor( &_samplers.resource_desc );
            rdi::device::DestroySampler( &_samplers.trilinear );
            rdi::device::DestroySampler( &_samplers.bilinear );
            rdi::device::DestroySampler( &_samplers.linear );
            rdi::device::DestroySampler( &_samplers.point );
        }

        rdi::DestroyPipeline( &_pipeline_blit );
        rdi::DestroyPipeline( &_pipeline_main );

        PostProcessPass::_ShutDown( &_post_process );

        rdi::device::DestroyTexture( &_bg_texture );
        rdi::device::DestroyTexture( &_bg_mask );
        rdi::device::DestroyTexture( &_swap_texture );
        rdi::device::DestroyTexture( &_color_texture );
        rdi::device::DestroyTexture( &_fft_texture );
        //rdi::device::DestroyTexture( &_logo_texture );
        rdi::device::DestroyTexture( &_spis_texture );
        rdi::DestroyRenderSource( &_fullscreen_quad );

        // ---
        rdi::Shutdown();

        ResourceManager* rsMan = GResourceManager();
        ResourceManager::shutdown( &rsMan );
        bxConfig::global_deinit();
    }

    bool NoGodsNoMasters::update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        bxInput_Keyboard* kbd = &win->input.kbd;
        if( bxInput_isKeyPressedOnce( kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        if( _next_song == UINT32_MAX )
        {
            if( bxInput_isKeyPressedOnce( kbd, '1' ) )
                _next_song = 0;
            if( bxInput_isKeyPressedOnce( kbd, '2' ) )
                _next_song = 1;
            if( bxInput_isKeyPressedOnce( kbd, '3' ) )
                _next_song = 2;
            if( bxInput_isKeyPressedOnce( kbd, '4' ) )
                _next_song = 3;
            if( bxInput_isKeyPressedOnce( kbd, '5' ) )
                _next_song = 4;
            if( bxInput_isKeyPressedOnce( kbd, '6' ) )
                _next_song = 5;
            if( bxInput_isKeyPressedOnce( kbd, '7' ) )
                _next_song = 6;
            if( bxInput_isKeyPressedOnce( kbd, '0' ) )
                _stop_request = true;
        }
        
        if( _next_song != UINT32_MAX )
        {
            _StopCurrentSong();
            SYS_ASSERT( _next_song < NUM_SONGS );
            BASS_ChannelPlay( _song_streams[_next_song], false );
            _current_song = _next_song;
            _next_song = UINT32_MAX;
        }
        else if( _stop_request )
        {
            _StopCurrentSong();
            _stop_request = false;
        }

        const u64 deltaTimeMS = deltaTimeUS / 1000;
        const float deltaTime = (float)( (double)deltaTimeMS * 0.001 );

        rdi::CommandQueue* cmdq = nullptr;
        rdi::frame::Begin( &cmdq );

        if( _current_song != UINT32_MAX )
        {
            memcpy( _fft_data_prev, _fft_data_curr, FFT_BINS * sizeof( f32 ) );
            int ierr = BASS_ChannelGetData( _song_streams[_current_song], _fft_data_curr, BASS_DATA_FFT512 );
            if( ierr != -1 )
            {
                for( int i = 0; i < FFT_BINS; ++i )
                {
                    float sampleValue = ::sqrt( _fft_data_curr[i] );
                    _fft_data_curr[i] = signalFilter_lowPass( sampleValue, _fft_data_prev[i], FFT_LOWPASS_RC, deltaTime );
                }

                rdi::context::UpdateTexture( cmdq, _fft_texture, _fft_data_curr );
                
                //ctx->backend()->updateTexture( __data.fftTexture, __data.fftDataCurr );
            }
        }

        

        rdi::context::ClearState( cmdq );
        
        rdi::BindResources( cmdq, _samplers.resource_desc );

        
        //{
        //    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_blit );
        //    rdi::SetResourceRO( rdesc, "gSrcTexture", &_bg_texture );
        //    rdi::BindPipeline( cmdq, _pipeline_blit, true );
        //
        //    rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
        //}

        {
            rdi::context::ChangeRenderTargets( cmdq, &_color_texture, 1, rdi::TextureDepth() );
            rdi::context::ClearColorBuffer( cmdq, _color_texture, 0.f, 0.f, 0.f, 1.f );

            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_main );
            rdi::SetResourceRO( rdesc, "gBgTex", &_bg_texture );
            rdi::SetResourceRO( rdesc, "gBgMaskTex", &_bg_mask );
            rdi::SetResourceRO( rdesc, "gFFTTex", &_fft_texture );
            rdi::SetResourceRO( rdesc, "gSpisTex", &_spis_texture );
            rdi::BindPipeline( cmdq, _pipeline_main, true );
            
            rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
        }

        {
            _post_process.DoToneMapping( cmdq, _swap_texture, _color_texture, deltaTime );
        }

        {
            rdi::context::ChangeToMainFramebuffer( cmdq );
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_blit );
            rdi::SetResourceRO( rdesc, "gSrcTexture", &_swap_texture );
            rdi::BindPipeline( cmdq, _pipeline_blit, true );

            const u32 winW = win->width;
            const u32 winH = win->height;
            const u32 texW = _color_texture.info.width;
            const u32 texH = _color_texture.info.height;
            const float aspect = (float)winW / (float)winH;
            gfx::Viewport vp = gfx::computeViewport( aspect, winW, winH, texW, texH );
            rdi::context::SetViewport( cmdq, vp );

            rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
        }

        rdi::context::Swap( cmdq );

        rdi::frame::End( &cmdq );

        return true;
    }

    void NoGodsNoMasters::DrawFullScreenQuad( rdi::CommandQueue* cmdq )
    {
        rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
    }

    void NoGodsNoMasters::_StopCurrentSong()
    {
        if( _current_song != UINT32_MAX )
        {
            BASS_ChannelStop( _song_streams[_current_song] );
            _current_song = UINT32_MAX;
        }
    }

}///

