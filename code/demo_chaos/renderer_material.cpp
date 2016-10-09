#include "renderer_material.h"


namespace bx{ namespace gfx{

bool MaterialContainer::alive( Material m ) const
{
    id_t id = makeId( m );
    return id_table::has( _id_to_index, id );
}

Material MaterialContainer::add( const char* name, ResourceDescriptor rdesc )
{
    id_t id = id_table::create( _id_to_index );
    u32 index = id.index;

    _index_to_id[index] = id;
    _rdescs[index] = rdesc;
    _names[index] = string::duplicate( nullptr, name );

    return makeMaterial( id );
}

void MaterialContainer::remove( Material* m )
{
    id_t id = makeId( *m );
    if( !id_table::has( _id_to_index, id ) )
        return;

    u32 index = id.index;
    string::free_and_null( (char**)&_names[index] );
    _rdescs[index] = BX_GFX_NULL_HANDLE;
    _index_to_id[index] = makeInvalidHandle<id_t>();

    m[0] = makeMaterial( makeInvalidHandle<id_t>() );
}

Material MaterialContainer::find( const char* name )
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
            return makeMaterial( _index_to_id[i] );
        }

        ++counter;
    }
    return makeMaterial( makeInvalidHandle<id_t>() );
}

u32 MaterialContainer::_GetIndex( Material m )
{
    id_t id = makeId( m );
    SYS_ASSERT( id_table::has( _id_to_index, id ) );
    return id.index;
}

ResourceDescriptor MaterialContainer::getResourceDesc( Material m )
{
    u32 index = _GetIndex( m );
    return _rdescs[index];
}

}}///