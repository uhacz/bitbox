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
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        
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
    
    rdi::ClearRenderTarget( cmdq, _rtarget_gbuffer, 0.f, 0.f, 0.f, 0.f, 1.f );
    rdi::BindRenderTarget( cmdq, _rtarget_gbuffer );

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
        rdi::ResourceBinding binding = rdi::ResourceBinding( "frame_data", rdi::EBindingType::UNIFORM ).Slot( SLOT_FRAME_DATA ).StageMask( rdi::EStage::ALL_STAGES_MASK );
        rdi::ResourceLayout layout = {};
        layout.bindings = &binding;
        layout.num_bindings = 1;
        pass->_rdesc_frame_data = rdi::CreateResourceDescriptor( layout );
        rdi::SetConstantBuffer( pass->_rdesc_frame_data, "frame_data", &pass->_cbuffer_frame_data );
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
        FrameData fdata = {};
        storeXYZ( camera.worldEye(), fdata.camera_eye.xyz );
        fdata.sun_color = float3_t( 1.0f, 1.0f, 1.0f );
        fdata.sun_intensity = 1.f;

        //const Vector3 L = normalize( mulAsVec4( camera.view, Vector3( 1.f, 1.f, 0.f ) ) );
        const Vector3 L = normalize( Vector3( 1.f, 1.f, 0.f ) );
        storeXYZ( L, fdata.vs_sun_L.xyz );

        rdi::context::UpdateCBuffer( cmdq, _cbuffer_fdata, &fdata );
    }
}

void LightPass::Flush( rdi::CommandQueue* cmdq, rdi::TextureRW outputTexture, rdi::RenderTarget gbuffer )
{
    float rgbad[] = { 0.f, 0.f, 0.f, 0.f, 1.f };
    rdi::context::ClearBuffers( cmdq, &outputTexture, 1, rdi::TextureDepth(), rgbad, 1, 0 );
    rdi::context::ChangeRenderTargets( cmdq, &outputTexture, 1, rdi::TextureDepth() );

    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline );
    rdi::SetResourceRO( rdesc, "gbuffer_albedo_spec", &rdi::GetTexture( gbuffer, 0 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wpos_rough", &rdi::GetTexture( gbuffer, 1 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wnrm_metal", &rdi::GetTexture( gbuffer, 2 ) );

    rdi::BindPipeline( cmdq, _pipeline, true );

    MeshHandle hmesh_fullscreen_quad = GMeshManager()->Find( ":fullscreen_quad" );
    rdi::RenderSource rsource_fullscreen_quad = GMeshManager()->RenderSource( hmesh_fullscreen_quad );
    rdi::BindRenderSource( cmdq, rsource_fullscreen_quad );
    rdi::SubmitRenderSource( cmdq, rsource_fullscreen_quad );
}

void LightPass::_StartUp( LightPass* pass )
{
    rdi::ShaderFile* shf = rdi::ShaderFileLoad( "shader/bin/deffered_lighting.shader", GResourceManager() );

    rdi::PipelineDesc pipeline_desc;
    pipeline_desc.Shader( shf, "lighting" );

    pass->_pipeline = rdi::CreatePipeline( pipeline_desc );
    pass->_cbuffer_fdata = rdi::device::CreateConstantBuffer( sizeof( FrameData ), nullptr );

    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( pass->_pipeline );
    rdi::SetConstantBuffer( rdesc, "FrameData", &pass->_cbuffer_fdata );

    rdi::ShaderFileUnload( &shf, GResourceManager() );
}

void LightPass::_ShutDown( LightPass* pass )
{
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_fdata );
    rdi::DestroyPipeline( &pass->_pipeline );
}
}}///
