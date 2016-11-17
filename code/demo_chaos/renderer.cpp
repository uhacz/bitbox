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
        _shared_mesh.add( ":fullscreen_quad", rsource );
    }

    {//// poly shapes
        bxPolyShape polyShape;
        bxPolyShape_createBox( &polyShape, 1 );
        rdi::RenderSource rsource_box = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        bxPolyShape_createShpere( &polyShape, 8 );
        rdi::RenderSource rsource_sphere = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        _shared_mesh.add( ":sphere", rsource_sphere );
        _shared_mesh.add( ":box", rsource_box );
    }
}

void Renderer::ShutDown( ResourceManager* resourceManager )
{
    {
        rdi::RenderSource rs;
        
        _shared_mesh.remove( ":box", &rs );
        rdi::DestroyRenderSource( &rs );
        
        _shared_mesh.remove( ":sphere", &rs );
        rdi::DestroyRenderSource( &rs );
        
        _shared_mesh.remove( ":fullscreen_quad", &rs );
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

    rdi::RenderSource rsource_fullscreen_quad = _shared_mesh.query( ":fullscreen_quad" );
    rdi::BindRenderSource( cmdq, rsource_fullscreen_quad );
    rdi::SubmitRenderSource( cmdq, rsource_fullscreen_quad );

}

}}///
