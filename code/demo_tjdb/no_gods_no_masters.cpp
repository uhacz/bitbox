#include "no_gods_no_masters.h"
#include <system\window.h>
#include <util/config.h>

namespace tjdb
{
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
        _logo_texture = LoadTextureFromFile( "texture/tjdb/okladka_logo.DDS" );
        _spis_texture = LoadTextureFromFile( "texture/tjdb/okladka_spis.DDS" );


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


        return true;
    }

    void NoGodsNoMasters::shutdown()
    {
        {
            rdi::DestroyResourceDescriptor( &_samplers.resource_desc );
            rdi::device::DestroySampler( &_samplers.trilinear );
            rdi::device::DestroySampler( &_samplers.bilinear );
            rdi::device::DestroySampler( &_samplers.linear );
            rdi::device::DestroySampler( &_samplers.point );
        }

        rdi::DestroyPipeline( &_pipeline_blit );
        rdi::DestroyPipeline( &_pipeline_main );
        rdi::device::DestroyTexture( &_bg_texture );
        rdi::device::DestroyTexture( &_logo_texture );
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
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const u64 deltaTimeMS = deltaTimeUS / 1000;

        rdi::CommandQueue* cmdq = nullptr;
        rdi::frame::Begin( &cmdq );

        rdi::context::ClearState( cmdq );
        
        rdi::BindResources( cmdq, _samplers.resource_desc );

        
        rdi::context::ChangeToMainFramebuffer( cmdq );
        //{
        //    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_blit );
        //    rdi::SetResourceRO( rdesc, "gSrcTexture", &_bg_texture );
        //    rdi::BindPipeline( cmdq, _pipeline_blit, true );
        //
        //    rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
        //}

        {
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_main );
            rdi::SetResourceRO( rdesc, "gBgTex", &_bg_texture );
            rdi::SetResourceRO( rdesc, "gLogoTex", &_logo_texture );
            rdi::SetResourceRO( rdesc, "gSpisTex", &_spis_texture );
            rdi::BindPipeline( cmdq, _pipeline_main, true );
            
            rdi::DrawFullscreenQuad( cmdq, _fullscreen_quad );
        }

        rdi::context::Swap( cmdq );

        rdi::frame::End( &cmdq );

        return true;
    }

}///

