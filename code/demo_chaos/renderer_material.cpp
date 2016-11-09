#include "renderer_material.h"


namespace bx{ namespace gfx{

bool MaterialContainer::Alive( MaterialID m ) const
{
    id_t id = MakeId( m );
    return id_table::has( _id_to_index, id );
}

MaterialID MaterialContainer::Add( const char* name, const Material& mat )
{
    id_t id = id_table::create( _id_to_index );
    u32 index = id.index;

    _index_to_id[index] = id;
    _material[index] = mat;
    _names[index] = string::duplicate( nullptr, name );

    return MakeMaterial( id );
}

void MaterialContainer::Remove( MaterialID* m )
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

MaterialID MaterialContainer::Find( const char* name ) const
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

u32 MaterialContainer::_GetIndex( MaterialID m ) const
{
    id_t id = MakeId( m );
    SYS_ASSERT( id_table::has( _id_to_index, id ) );
    return id.index;
}

const Material& MaterialContainer::GetMaterial( MaterialID id ) const
{
    u32 index = _GetIndex( id );
    return _material[index];
}

void MaterialContainer::SetMaterialData( MaterialID id, const MaterialData& data )
{
    u32 index = _GetIndex( id );
    _material[index].data = data;
}

void Clear( Material* mat )
{
    memset( mat, 0x00, sizeof( *mat ) );
}

MaterialID CreateMaterial( MaterialContainer* container, const char* name, const MaterialData& data, const MaterialTextures& textures )
{
    MaterialID found_id = container->Find( name );
    SYS_ASSERT( !IsValid( found_id ) );
    
    Material mat;
    Clear( &mat );
    
    mat.data = data;
    mat.data_cbuffer = rdi::device::CreateConstantBuffer( sizeof( MaterialData ) );

    if( textures.diffuse_tex )
    {
        TextureManager* texture_manager = GTextureManager();
        mat.htexture.diffuse_tex = texture_manager->CreateFromFile( textures.diffuse_tex );
        mat.htexture.specular_tex = texture_manager->CreateFromFile( textures.specular_tex );
        mat.htexture.roughness_tex = texture_manager->CreateFromFile( textures.roughness_tex );
        mat.htexture.metallic_tex = texture_manager->CreateFromFile( textures.metallic_tex );
    }

    return container->Add( name, mat );
}

void DestroyMaterial( MaterialContainer* container, MaterialID* id )
{
    if( !container->Alive( *id ) )
        return;

    Material mat = container->GetMaterial( *id );
    TextureManager* texture_manager = GTextureManager();
    texture_manager->Release( mat.htexture.diffuse_tex );
    texture_manager->Release( mat.htexture.specular_tex );
    texture_manager->Release( mat.htexture.roughness_tex );
    texture_manager->Release( mat.htexture.metallic_tex );

    rdi::device::DestroyConstantBuffer( &mat.data_cbuffer );

    container->Remove( id );
}

void FillResourceDescriptor( rdi::ResourceDescriptor rdesc, const Material& mat )
{
    
}

}}///