#include "renderer_material.h"

namespace bx{ namespace gfx{

bool MaterialManager::Alive( MaterialID m ) const
{
    id_t id = MakeId( m );
    return id_table::has( _id_to_index, id );
}

MaterialID MaterialManager::Add( const char* name )
{
    id_t id = id_table::create( _id_to_index );
    u32 index = id.index;

    _index_to_id[index] = id;
    _material[index] = {};
    _names[index] = string::duplicate( nullptr, name );

    return MakeMaterial( id );
}

void MaterialManager::Remove( MaterialID* m )
{
    id_t id = MakeId( *m );
    if( !id_table::has( _id_to_index, id ) )
        return;

    u32 index = id.index;
    string::free_and_null( (char**)&_names[index] );
    _material[index] = {};
    _index_to_id[index] = makeInvalidHandle<id_t>();

    m[0] = MakeMaterial( makeInvalidHandle<id_t>() );
}

MaterialID MaterialManager::Find( const char* name ) const
{
    const u32 n = id_table::size( _id_to_index );
    u32 counter = 0;
    for( u32 i = 0; i < n && counter < n; ++i )
    {
        const char* current_name = _names[i];
        if( !current_name )
            continue;

        if( string::equal( current_name, name ) )
        {
            SYS_ASSERT( id_table::has( _id_to_index, _index_to_id[i] ) );
            return MakeMaterial( _index_to_id[i] );
        }

        ++counter;
    }
    return MakeMaterial( makeInvalidHandle<id_t>() );
}

u32 MaterialManager::_GetIndex( MaterialID m ) const
{
    id_t id = MakeId( m );
    SYS_ASSERT( id_table::has( _id_to_index, id ) );
    return id.index;
}

void MaterialManager::_Set( MaterialID id, const Material& m )
{
    u32 index = _GetIndex( id );
    _material[index] = m;
}

const Material& MaterialManager::Get( MaterialID id ) const
{
    u32 index = _GetIndex( id );
    return _material[index];
}

rdi::ResourceDescriptor MaterialManager::GetResourceDescriptor( MaterialID id ) const
{
    u32 index = _GetIndex( id );
    return _material[index].resource_desc;
}

void MaterialManager::SetMaterialData( MaterialID id, const MaterialData& data )
{
    u32 index = _GetIndex( id );
    _material[index].data = data;
}

MaterialID MaterialManager::Create( const char* name, const MaterialData& data, const MaterialTextures& textures )
{
    MaterialID found_id = Find( name );
    SYS_ASSERT( !IsValid( found_id ) );

    MaterialID id = Add( name );

    Material mat;
    Clear( &mat );

    mat.data = data;
    mat.data_cbuffer = rdi::device::CreateConstantBuffer( sizeof( MaterialData ), &data );

    rdi::ResourceLayout resource_layout = material_resource_layout::laytout_notex;
    {
        TextureManager* texture_manager = GTextureManager();
        mat.htexture.diffuse_tex = texture_manager->CreateFromFile( textures.diffuse_tex );
        mat.htexture.specular_tex = texture_manager->CreateFromFile( textures.specular_tex );
        mat.htexture.roughness_tex = texture_manager->CreateFromFile( textures.roughness_tex );
        mat.htexture.metallic_tex = texture_manager->CreateFromFile( textures.metallic_tex );

        resource_layout = material_resource_layout::laytout_tex;
    }

    mat.resource_desc = rdi::CreateResourceDescriptor( resource_layout );
    FillResourceDescriptor( mat.resource_desc, mat );

    _Set( id, mat );

    return id;
}

void MaterialManager::Destroy( MaterialID* id )
{
    if( !Alive( *id ) )
        return;

    Material mat = Get( *id );

    rdi::DestroyResourceDescriptor( &mat.resource_desc );

    TextureManager* texture_manager = GTextureManager();
    texture_manager->Release( mat.htexture.diffuse_tex );
    texture_manager->Release( mat.htexture.specular_tex );
    texture_manager->Release( mat.htexture.roughness_tex );
    texture_manager->Release( mat.htexture.metallic_tex );

    rdi::device::DestroyConstantBuffer( &mat.data_cbuffer );

    Remove( id );
}

void Clear( Material* mat )
{
    memset( mat, 0x00, sizeof( *mat ) );
}

static MaterialManager* g_material_manager = nullptr;

void MaterialManagerStartUp()
{
    SYS_ASSERT( g_material_manager == nullptr );
    g_material_manager = BX_NEW( bxDefaultAllocator(), MaterialManager );
}

void MaterialManagerShutDown()
{
    SYS_ASSERT( g_material_manager != nullptr );
    BX_DELETE0( bxDefaultAllocator(), g_material_manager );
}
MaterialManager* GMaterialManager()
{
    return g_material_manager;
}

//////////////////////////////////////////////////////////////////////////
void FillResourceDescriptor( rdi::ResourceDescriptor rdesc, const Material& mat )
{
    rdi::SetConstantBufferByIndex( rdesc, 0, &mat.data_cbuffer );
    if( GHandle()->Alive( mat.htexture.diffuse_tex ) )
    {
        rdi::TextureRO* diffuse_tex   = GHandle()->DataAs<rdi::TextureRO*>( mat.htexture.diffuse_tex );
        rdi::TextureRO* specular_tex  = GHandle()->DataAs<rdi::TextureRO*>( mat.htexture.specular_tex );
        rdi::TextureRO* roughness_tex = GHandle()->DataAs<rdi::TextureRO*>( mat.htexture.roughness_tex );
        rdi::TextureRO* metallic_tex  = GHandle()->DataAs<rdi::TextureRO*>( mat.htexture.metallic_tex );
        
        rdi::SetResourceROByIndex( rdesc, 1, diffuse_tex );
        rdi::SetResourceROByIndex( rdesc, 2, specular_tex );
        rdi::SetResourceROByIndex( rdesc, 3, roughness_tex );
        rdi::SetResourceROByIndex( rdesc, 4, metallic_tex );
    }
}

//////////////////////////////////////////////////////////////////////////


MaterialID MaterialManager1::Create( const char* name, const MaterialDesc& desc )
{
    MaterialID material_id = Find( name );
    if( IsValid( material_id ) )
    {
        return material_id;
    }

    ResourceManager* resource_manager = GResourceManager();

    ResourceID resource_id = ResourceManager::createResourceID( name, "material" );
    bool create_material = false;
    _lock.lock();
    ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
    if( !resource_ptr )
    {
        id_t id = id_table::create( _id_to_index );
        GResourceManager()->insertResource( resource_id, ResourcePtr( id.hash ) );
        material_id.i = id.hash;
        create_material = true;
    }
    else
    {
        material_id.i = (u32)resource_ptr;             
    }
    _lock.unlock();

    if( create_material )
    {
        id_t id = MakeId( material_id );
        a
    }


    return material_id;
}

void MaterialManager1::Destroy( MaterialID id )
{

}

MaterialID MaterialManager1::Find( const char* name )
{
    MaterialID id;
    
    ResourceID resource_id = ResourceManager::createResourceID( name, "material" );
    ResourcePtr resource_ptr = GResourceManager()->acquireResource( resource_id );
    if( resource_ptr )
    {
        id.i = (u32)resource_ptr;
    }

    return id;
}

MaterialPipeline MaterialManager1::Pipeline( MaterialID id ) const
{

}

//////////////////////////////////////////////////////////////////////////
MaterialManager1* g_material_manager1 = nullptr;
void MaterialManager1::_StartUp()
{
    SYS_ASSERT( g_material_manager1 == nullptr );
    g_material_manager1 = BX_NEW( bxDefaultAllocator(), MaterialManager1 );

    rdi::ShaderFile* sfile = rdi::ShaderFileLoad( "shader/bin/deffered.shader", GResourceManager() );
    
    rdi::PipelineDesc pipeline_desc = {};

    pipeline_desc.Shader( sfile, "geometry_notexture" );
    g_material_manager1->_pipeline_notex = rdi::CreatePipeline( pipeline_desc );
    SYS_ASSERT( g_material_manager1->_pipeline_notex != BX_RDI_NULL_HANDLE );

    pipeline_desc.Shader( sfile, "geometry_texture" );
    g_material_manager1->_pipeline_tex = rdi::CreatePipeline( pipeline_desc );
    SYS_ASSERT( g_material_manager1->_pipeline_tex != BX_RDI_NULL_HANDLE );
}

void MaterialManager1::_ShutDown()
{
    SYS_ASSERT( g_material_manager1 != nullptr );
    
    {
        rdi::DestroyPipeline( &g_material_manager1->_pipeline_tex );
        rdi::DestroyPipeline( &g_material_manager1->_pipeline_notex );
    }

    BX_DELETE0( bxDefaultAllocator(), g_material_manager1 );
}
MaterialManager1* GMaterialManager1()
{
    return g_material_manager1;
}


}}///