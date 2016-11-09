#include "renderer_material.h"


namespace bx{ namespace gfx{

bool MaterialContainer::Alive( MaterialID m ) const
{
    id_t id = MakeId( m );
    return id_table::has( _id_to_index, id );
}

MaterialID MaterialContainer::Add( const char* name, const Material& mat, const MaterialTextureResources& resources )
{
    id_t id = id_table::create( _id_to_index );
    u32 index = id.index;

    _index_to_id[index] = id;
    //_rdescs[index] = rdesc;
    _material[index] = mat;
    _resources[index] = resources;
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
    //_rdescs[index] = BX_RDI_NULL_HANDLE;
    _material[index] = {};
    _resources[index] = {};
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

MaterialTextureResources& MaterialContainer::GetResources( MaterialID id )
{
    u32 index = _GetIndex( id );
    return _resources[index];
}


void MaterialContainer::SetMaterialData( MaterialID id, const MaterialData& data )
{
    u32 index = _GetIndex( id );
    _material[index].data = data;
}

MaterialID CreateMaterial( ResourceManager* resourceManager, const char* name, const MaterialData& data, const MaterialTextures& textures )
{
        
}

void DestroyMaterial( ResourceManager* resourceManager, MaterialID* id )
{

}

//rdi::ResourceDescriptor MaterialContainer::getResourceDesc( MaterialID m )
//{
//    u32 index = _GetIndex( m );
//    return _rdescs[index];
//}

}}///

//namespace bx{ namespace gfx{
//
//namespace MATERIALS
//{
//
//    
//}///
//
//}}///
