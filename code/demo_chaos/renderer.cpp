#include "renderer.h"
#include <util\common.h>
#include <util\buffer_utils.h>
#include <rdi/rdi.h>

#include "renderer_scene.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"

#include <shaders/hlsl/sys/binding_map.h>


namespace bx{
namespace gfx{
    
//////////////////////////////////////////////////////////////////////////
struct GBuffer
{

};

//////////////////////////////////////////////////////////////////////////
struct RendererImpl
{
    struct EFramebuffer
    {
        enum Enum
        {
            ALBEDO_SPECULAR,
            VPOSITION_ROUGHNESS,
            VNORMAL_METALIC,
        };
    };
    rdi::RenderTarget renderTarget;
    rdi::Pipeline pip_copy_tex_rgba = BX_RDI_NULL_HANDLE;
    rdi::Pipeline pip_material_notex = BX_RDI_NULL_HANDLE;

    rdi::ResourceDescriptor rdesc_copy_tex = BX_RDI_NULL_HANDLE;
    rdi::ResourceDescriptor rdesc_material_notex = BX_RDI_NULL_HANDLE;

    rdi::ShaderFile* sfile_texutil = nullptr;
    rdi::ShaderFile* sfile_deffered = nullptr;
};

namespace renderer
{

static SharedMeshContainer* g_mesh_container = nullptr;
static MaterialContainer* g_material_container = nullptr;
    
//////////////////////////////////////////////////////////////////////////
Renderer startup()
{
    g_mesh_container = BX_NEW( bxDefaultAllocator(), SharedMeshContainer );
    g_material_container = BX_NEW( bxDefaultAllocator(), MaterialContainer );
    SceneImpl::StartUp();
    return nullptr;
}

void shutdown()
{
    SceneImpl::ShutDown();
    BX_DELETE0( bxDefaultAllocator(), g_material_container );
    BX_DELETE0( bxDefaultAllocator(), g_mesh_container );
}

MaterialID createMaterial( const char* name, const MaterialDesc& desc, const MaterialTextureNames* textures )
{
    //rdi::ResourceLayout resourceLayout = {};
    //resourceLayout.bindings = g_material_bindings;
    //resourceLayout.num_bindings = ( textures ) ? 3 : 1;
    //rdi::ResourceDescriptor resourceDesc = rdi::CreateResourceDescriptor( resourceLayout, nullptr );

    //rdi::ConstantBuffer cbuffer = rdi::device::CreateConstantBuffer( sizeof( MaterialDesc ) );
    //rdi::SetConstantBuffer( resourceDesc, cbuffer, rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_DATA );

    //if( textures )
    //{
    //    rdi::SetResourceRO( resourceDesc, &textures->diffuse_tex  , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE0 );
    //    rdi::SetResourceRO( resourceDesc, &textures->specular_tex , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE1 );
    //    rdi::SetResourceRO( resourceDesc, &textures->roughness_tex, rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE2 );
    //    rdi::SetResourceRO( resourceDesc, &textures->metallic_tex , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE3 );
    //}
      
    MaterialID m;
    m.i = 0;
    return m;
    //return g_material_container.add( name, resourceDesc );
}

void destroyMaterial( MaterialID* m )
{
    if( !g_material_container->alive( *m ) )
        return;

    rdi::ResourceDescriptor rdesc = g_material_container->getResourceDesc( *m );
    g_material_container->remove( m );


    rdi::DestroyResourceDescriptor( &rdesc, nullptr );
}

MaterialID findMaterial( const char* name )
{
    return g_material_container->find( name );
}

void addSharedRenderSource( const char* name, rdi::RenderSource rsource )
{
    g_mesh_container->add( name, rsource );
}

rdi::RenderSource findSharedRenderSource( const char* name )
{
    return g_mesh_container->query( name );
}

Scene createScene( const char* name )
{
    SceneImpl* impl = BX_NEW( bxDefaultAllocator(), SceneImpl );
    impl->prepare( name, nullptr );
    return impl;
}

void destroyScene( Scene* scene )
{
    SceneImpl* impl = scene[0];
    if( !impl )
        return;

    impl->unprepare();
    BX_DELETE0( bxDefaultAllocator(), scene[0] );
}

}///

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void Renderer::StartUp( const RendererDesc& desc, ResourceManager* resourceManager )
{
    _desc = desc;
    {
        rdi::RenderTargetDesc rt_desc = {};
        rt_desc.Size( 1920, 1080 );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        a
        _render_target = rdi::CreateRenderTarget( rt_desc );
    }
    
    _shf_texutil = rdi::ShaderFileLoad( "shader/bin/texture_utils.shader", resourceManager );
    {
        rdi::PipelineDesc pipeline_desc = {};
        pipeline_desc.Shader( _shf_texutil, "copy_rgba" );
        _pipeline_copy_texture_rgba = rdi::CreatePipeline( pipeline_desc );
    }
}

void Renderer::ShutDown( ResourceManager* resourceManager )
{

}

Scene Renderer::CreateScene( const char* name )
{

}

void Renderer::DestroyScene( Scene* scene )
{

}

void Renderer::RasterizeFramebuffer( const rdi::ResourceRO source, const Camera& camera )
{

}

}}///
