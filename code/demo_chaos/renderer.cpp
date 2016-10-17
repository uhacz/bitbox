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

struct RenderingUtils
{
    struct Pass
    {
        rdi::Pipeline pipeline = BX_RDI_NULL_HANDLE;
        rdi::ResourceDescriptor resource_desc = BX_RDI_NULL_HANDLE;
    };

    Pass _texture_copy_rgba;

};

namespace renderer
{
    static SharedMeshContainer g_mesh_container = {};
    static MaterialContainer g_material_container = {};
    
//////////////////////////////////////////////////////////////////////////
void startup()
{

}

void shutdown()
{

}

static rdi::ResourceBinding g_material_bindings[] =
{
    rdi::ResourceBinding( rdi::EBindingType::UNIFORM  ).StageMask( rdi::EStage::PIXEL_MASK ).FirstSlotAndCount( SLOT_MATERIAL_DATA, 1 ),
    rdi::ResourceBinding( rdi::EBindingType::READ_ONLY ).StageMask( rdi::EStage::PIXEL_MASK ).FirstSlotAndCount( SLOT_MATERIAL_TEXTURE0, SLOT_MATERIAL_TEXTURE_COUNT ),
    rdi::ResourceBinding( rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::PIXEL_MASK ).FirstSlotAndCount( 0, 1 ),
};
Material createMaterial( const char* name, const MaterialDesc& desc, const MaterialTextureNames* textures )
{
    rdi::ResourceLayout resourceLayout = {};
    resourceLayout.bindings = g_material_bindings;
    resourceLayout.num_bindings = ( textures ) ? 3 : 1;
    rdi::ResourceDescriptor resourceDesc = rdi::CreateResourceDescriptor( resourceLayout, nullptr );

    rdi::ConstantBuffer cbuffer = rdi::device::CreateConstantBuffer( sizeof( MaterialDesc ) );
    rdi::SetConstantBuffer( resourceDesc, cbuffer, rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_DATA );

    //if( textures )
    //{
    //    rdi::SetResourceRO( resourceDesc, &textures->diffuse_tex  , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE0 );
    //    rdi::SetResourceRO( resourceDesc, &textures->specular_tex , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE1 );
    //    rdi::SetResourceRO( resourceDesc, &textures->roughness_tex, rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE2 );
    //    rdi::SetResourceRO( resourceDesc, &textures->metallic_tex , rdi::EStage::PIXEL_MASK, SLOT_MATERIAL_TEXTURE3 );
    //}
        
    return g_material_container.add( name, resourceDesc );
}

void destroyMaterial( Material* m )
{
    if( !g_material_container.alive( *m ) )
        return;

    rdi::ResourceDescriptor rdesc = g_material_container.getResourceDesc( *m );
    g_material_container.remove( m );


    rdi::DestroyResourceDescriptor( &rdesc, nullptr );
}

Material findMaterial( const char* name )
{
    return g_material_container.find( name );
}

void addSharedRenderSource( const char* name, rdi::RenderSource rsource )
{
    g_mesh_container.add( name, rsource );
}

rdi::RenderSource findSharedRenderSource( const char* name )
{
    u32 index = g_mesh_container.find( name );
    return ( index != UINT32_MAX ) ? g_mesh_container.getRenderSource( index ) : BX_RDI_NULL_HANDLE;
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

}}///
