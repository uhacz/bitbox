#include "renderer.h"
#include <util\common.h>
#include <util\buffer_utils.h>
#include <util\poly\poly_shape.h>
#include <rdi/rdi.h>

#include "renderer_scene.h"
#include "renderer_scene_actor.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace bx{ namespace gfx{
void Renderer::StartUp( const RendererDesc& desc, ResourceManager* resourceManager )
{
    TextureManager::_StartUp();
    MaterialManager::_StartUp();
    MeshManager::_StartUp();
    {
        _actor_handle_manager = BX_NEW( bxDefaultAllocator(), ActorHandleManager );
        _actor_handle_manager->StartUp();
    }

    _desc = desc;
    {
        rdi::RenderTargetDesc rt_desc = {};
        rt_desc.Size( desc.framebuffer_width, desc.framebuffer_height );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) ); // COLOR
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) ); // SWAP
        
        
        _render_target = rdi::CreateRenderTarget( rt_desc );
    }
    
    _shf_texutil = rdi::ShaderFileLoad( "shader/bin/texture_utils.shader", resourceManager );
    {
        rdi::PipelineDesc pipeline_desc = {};
        pipeline_desc.Shader( _shf_texutil, "copy_rgba" );
        _pipeline_copy_texture_rgba = rdi::CreatePipeline( pipeline_desc );
    }

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

    {
        const float vertices_pos[] =
        {
            -1.f, -1.f, 0.f,
            1.f , -1.f, 0.f,
            1.f , 1.f , 0.f,

            -1.f, -1.f, 0.f,
            1.f , 1.f , 0.f,
            -1.f, 1.f , 0.f,
        };
        const float vertices_uv[] =
        {
            0.f, 0.f,
            1.f, 0.f,
            1.f, 1.f,

            0.f, 0.f,
            1.f, 1.f,
            0.f, 1.f,
        };

        rdi::RenderSourceDesc rsource_desc = {};
        rsource_desc.Count( 6 );
        rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::POSITION ).DataType( rdi::EDataType::FLOAT, 3 ), vertices_pos );
        rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::TEXCOORD0 ).DataType( rdi::EDataType::FLOAT, 2 ), vertices_uv );
        rdi::RenderSource rsource = rdi::CreateRenderSource( rsource_desc );
        MeshHandle hmesh = GMeshManager()->Add( ":fullscreen_quad" );
        GMeshManager()->SetRenderSource( hmesh, rsource );
    }

    {//// poly shapes
        bxPolyShape polyShape;
        bxPolyShape_createBox( &polyShape, 1 );
        rdi::RenderSource rsource_box = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        bxPolyShape_createShpere( &polyShape, 8 );
        rdi::RenderSource rsource_sphere = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        MeshHandle hmesh = GMeshManager()->Add( ":sphere" );
        GMeshManager()->SetRenderSource( hmesh, rsource_sphere );
        GMeshManager()->SetLocalAABB( hmesh, bxAABB( Vector3( -0.5f ), Vector3( 0.5f ) ) );

        hmesh = GMeshManager()->Add( ":box" );
        GMeshManager()->SetRenderSource( hmesh, rsource_box );
        GMeshManager()->SetLocalAABB( hmesh, bxAABB( Vector3( -0.5f ), Vector3( 0.5f ) ) );
    }
}

void Renderer::ShutDown( ResourceManager* resourceManager )
{
    {
        rdi::RenderSource rs;
        GMeshManager()->RemoveByName( ":box", &rs );
        rdi::DestroyRenderSource( &rs );

        GMeshManager()->RemoveByName( ":sphere", &rs );
        rdi::DestroyRenderSource( &rs );

        GMeshManager()->RemoveByName( ":fullscreen_quad", &rs );
        rdi::DestroyRenderSource( &rs );
    }

    {
        rdi::DestroyResourceDescriptor( &_samplers.resource_desc );
        rdi::device::DestroySampler( &_samplers.trilinear );
        rdi::device::DestroySampler( &_samplers.bilinear );
        rdi::device::DestroySampler( &_samplers.linear );
        rdi::device::DestroySampler( &_samplers.point );
    }

    rdi::DestroyPipeline( &_pipeline_copy_texture_rgba );
    rdi::DestroyRenderTarget( &_render_target );

    rdi::ShaderFileUnload( &_shf_texutil, resourceManager );

    MeshManager::_ShutDown();
    MaterialManager::_ShutDown();
    TextureManager::_ShutDown();

    {
        _actor_handle_manager->ShutDown();
        BX_DELETE0( bxDefaultAllocator(), _actor_handle_manager );
    }
}

Scene Renderer::CreateScene( const char* name )
{
    SceneImpl* impl = BX_NEW( bxDefaultAllocator(), SceneImpl );
    impl->_handle_manager = _actor_handle_manager;
    impl->Prepare( name, nullptr );

    return impl;
}

void Renderer::DestroyScene( Scene* scene )
{
    SceneImpl* impl = scene[0];
    if( !impl )
        return;

    impl->Unprepare();
    BX_DELETE0( bxDefaultAllocator(), scene[0] );
}

void Renderer::BeginFrame( rdi::CommandQueue* command_queue )
{
    rdi::context::ClearState( command_queue );
    rdi::BindResources( command_queue, _samplers.resource_desc );
}

void Renderer::EndFrame( rdi::CommandQueue* command_queue )
{
    rdi::context::Swap( command_queue );
}

void Renderer::RasterizeFramebuffer( rdi::CommandQueue* cmdq, const rdi::ResourceRO source, const Camera& camera, u32 windowW, u32 windowH )
{
    const rdi::Viewport screen_viewport = gfx::computeViewport( camera, windowW, windowH, _desc.framebuffer_width, _desc.framebuffer_height );
    rdi::context::ChangeToMainFramebuffer( cmdq );
    rdi::context::SetViewport( cmdq, screen_viewport );
    
    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_copy_texture_rgba );
    rdi::SetResourceRO( rdesc, "gtexture", &source );
    //rdi::SetSampler( rdesc, "gsampler", &_samplers.point );
    
    rdi::BindPipeline( cmdq, _pipeline_copy_texture_rgba, true );
    rdi::BindResources( cmdq, rdesc );

    MeshHandle hmesh_fullscreen_quad = GMeshManager()->Find( ":fullscreen_quad" );
    rdi::RenderSource rsource_fullscreen_quad = GMeshManager()->RenderSource( hmesh_fullscreen_quad );
    rdi::BindRenderSource( cmdq, rsource_fullscreen_quad );
    rdi::SubmitRenderSource( cmdq, rsource_fullscreen_quad );

}
void Renderer::DrawFullScreenQuad( rdi::CommandQueue* cmdq )
{
    MeshHandle hmesh_fullscreen_quad = GMeshManager()->Find( ":fullscreen_quad" );
    rdi::RenderSource rsource_fullscreen_quad = GMeshManager()->RenderSource( hmesh_fullscreen_quad );
    rdi::BindRenderSource( cmdq, rsource_fullscreen_quad );
    rdi::SubmitRenderSource( cmdq, rsource_fullscreen_quad );
}

}}///


namespace bx{ namespace gfx{

void GeometryPass::PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera )
{
    {
        FrameData fdata;
        fdata._view = camera.view;
        fdata._view_proj = camera.view_proj;

        rdi::context::UpdateCBuffer( cmdq, _cbuffer_frame_data, &fdata );
        //rdi::BindResources( cmdq, _rdesc_frame_data );
    }

    {
        _vertex_transform_data.Map( cmdq );

        rdi::ClearCommandBuffer( _command_buffer );
        rdi::BeginCommandBuffer( _command_buffer );

        scene->BuildCommandBuffer( _command_buffer, &_vertex_transform_data, camera );

        rdi::EndCommandBuffer( _command_buffer );

        _vertex_transform_data.Unmap( cmdq );
    }
}

void GeometryPass::Flush( rdi::CommandQueue* cmdq )
{
    rdi::BindResources( cmdq, _rdesc_frame_data );
    _vertex_transform_data.Bind( cmdq );
    
    rdi::BindRenderTarget( cmdq, _rtarget_gbuffer );
    rdi::ClearRenderTarget( cmdq, _rtarget_gbuffer, 5000.f, 6000.f, 8000.f, 1000.f, 1.f );

    rdi::SubmitCommandBuffer( cmdq, _command_buffer );
}

void GeometryPass::_StartUp( GeometryPass* pass )
{
    {
        rdi::RenderTargetDesc rt_desc = {};
        rt_desc.Size( 1920, 1080 );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Depth( rdi::EDataType::DEPTH32F );

        pass->_rtarget_gbuffer = rdi::CreateRenderTarget( rt_desc );
    }

    {
        pass->_cbuffer_frame_data = rdi::device::CreateConstantBuffer( sizeof( FrameData ) );
        rdi::ResourceBinding binding = rdi::ResourceBinding( "FrameData", rdi::EBindingType::UNIFORM ).Slot( SLOT_FRAME_DATA ).StageMask( rdi::EStage::ALL_STAGES_MASK );
        rdi::ResourceLayout layout = {};
        layout.bindings = &binding;
        layout.num_bindings = 1;
        pass->_rdesc_frame_data = rdi::CreateResourceDescriptor( layout );
        rdi::SetConstantBuffer( pass->_rdesc_frame_data, "FrameData", &pass->_cbuffer_frame_data );
    }

    {
        pass->_command_buffer = rdi::CreateCommandBuffer();
    }

    VertexTransformData::_Init( &pass->_vertex_transform_data, 1024 );

}

void GeometryPass::_ShutDown( GeometryPass* pass )
{
    VertexTransformData::_Deinit( &pass->_vertex_transform_data );

    rdi::DestroyCommandBuffer( &pass->_command_buffer );
    rdi::DestroyResourceDescriptor( &pass->_rdesc_frame_data );
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_frame_data );
    rdi::DestroyRenderTarget( &pass->_rtarget_gbuffer );
}


//////////////////////////////////////////////////////////////////////////
void LightPass::PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera )
{
    {
        LightPass::MaterialData mdata = {};
        storeXYZ( camera.worldEye(), mdata.camera_eye.xyzw );
        storeXYZ( camera.worldDir(), mdata.camera_dir.xyzw );
        mdata.sun_color = float4_t( 1.0f, 1.0f, 1.0f, 1.0 );
        mdata.sun_intensity = 110000.f;
        mdata.sky_intensity = 30000.f;

        //const Vector3 L = normalize( mulAsVec4( camera.view, Vector3( 1.f, 1.f, 0.f ) ) );
        const Vector3 L = normalize( Vector3( 1.f, 1.f, 1.f ) );
        storeXYZ( L, mdata.sun_L.xyzw );

        rdi::context::UpdateCBuffer( cmdq, _cbuffer_fdata, &mdata );
    }
}

void LightPass::Flush( rdi::CommandQueue* cmdq, rdi::TextureRW outputTexture, rdi::RenderTarget gbuffer )
{
    float rgbad[] = { 0.0f, 0.0f, 0.0f, 1.f, 1.f };
    rdi::context::ChangeRenderTargets( cmdq, &outputTexture, 1, rdi::TextureDepth() );
    rdi::context::ClearBuffers( cmdq, &outputTexture, 1, rdi::TextureDepth(), rgbad, 1, 0 );

    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline );
    rdi::SetResourceRO( rdesc, "gbuffer_albedo_spec", &rdi::GetTexture( gbuffer, 0 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wpos_rough", &rdi::GetTexture( gbuffer, 1 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wnrm_metal", &rdi::GetTexture( gbuffer, 2 ) );

    rdi::BindPipeline( cmdq, _pipeline, true );
    Renderer::DrawFullScreenQuad( cmdq );
}

void LightPass::_StartUp( LightPass* pass )
{
    rdi::ShaderFile* shf = rdi::ShaderFileLoad( "shader/bin/deffered_lighting.shader", GResourceManager() );

    rdi::PipelineDesc pipeline_desc;
    pipeline_desc.Shader( shf, "lighting" );

    pass->_pipeline = rdi::CreatePipeline( pipeline_desc );
    pass->_cbuffer_fdata = rdi::device::CreateConstantBuffer( sizeof( LightPass::MaterialData ), nullptr );

    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( pass->_pipeline );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_fdata );

    rdi::ShaderFileUnload( &shf, GResourceManager() );
}

void LightPass::_ShutDown( LightPass* pass )
{
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_fdata );
    rdi::DestroyPipeline( &pass->_pipeline );
}

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
        
        data.lum_tau = 15.f;
        data.auto_exposure_key_value = 0.30f;
        data.use_auto_exposure = 1;
        data.camera_aperture = 16.f;
        data.camera_shutterSpeed = 0.01f;
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
        _tm.data.input_size0 = float2_t( (float)outTexture.width, (float)outTexture.height );
        _tm.data.delta_time = deltaTime;
        rdi::context::UpdateCBuffer( cmdq, _tm.cbuffer_data, &_tm.cbuffer_data );


        const float clear_color[5] = { 0.f, 0.f, 0.f, 0.f, 1.0f };

        { // --- LuminanceMap
            ChangeTargetAndClear( cmdq, _tm.initial_luminance, clear_color );
            
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_luminance_map );
            rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
            rdi::BindPipeline( cmdq, _tm.pipeline_luminance_map, true );
            Renderer::DrawFullScreenQuad( cmdq );
        }

        { // --- AdaptLuminance
            ChangeTargetAndClear( cmdq, _tm.CurrLuminanceTexture(), clear_color );
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_adapt_luminance );
            rdi::SetResourceRO( rdesc, "tex_input0", &_tm.PrevLuminanceTexture() );
            rdi::SetResourceRO( rdesc, "tex_input1", &_tm.initial_luminance );
            rdi::BindPipeline( cmdq, _tm.pipeline_adapt_luminance, true );
            Renderer::DrawFullScreenQuad( cmdq );

            rdi::context::GenerateMipmaps( cmdq, _tm.CurrLuminanceTexture() );
        }

        { // --- Composite
            ChangeTargetAndClear( cmdq, outTexture, clear_color );
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_composite );
            rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
            rdi::SetResourceRO( rdesc, "tex_input1", &_tm.CurrLuminanceTexture() );
            rdi::BindPipeline( cmdq, _tm.pipeline_composite, true );
            Renderer::DrawFullScreenQuad( cmdq );
        }
    
        _tm.current_luminance_texture = !_tm.current_luminance_texture;
    }


}

}}///
