#include "renderer_material.h"


namespace bx{ namespace gfx{

bool MaterialContainer::alive( MaterialID m ) const
{
    id_t id = makeId( m );
    return id_table::has( _id_to_index, id );
}

MaterialID MaterialContainer::add( const char* name, rdi::ResourceDescriptor rdesc )
{
    id_t id = id_table::create( _id_to_index );
    u32 index = id.index;

    _index_to_id[index] = id;
    _rdescs[index] = rdesc;
    _names[index] = string::duplicate( nullptr, name );

    return makeMaterial( id );
}

void MaterialContainer::remove( MaterialID* m )
{
    id_t id = makeId( *m );
    if( !id_table::has( _id_to_index, id ) )
        return;

    u32 index = id.index;
    string::free_and_null( (char**)&_names[index] );
    _rdescs[index] = BX_RDI_NULL_HANDLE;
    _index_to_id[index] = makeInvalidHandle<id_t>();

    m[0] = makeMaterial( makeInvalidHandle<id_t>() );
}

MaterialID MaterialContainer::find( const char* name )
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

u32 MaterialContainer::_GetIndex( MaterialID m )
{
    id_t id = makeId( m );
    SYS_ASSERT( id_table::has( _id_to_index, id ) );
    return id.index;
}

rdi::ResourceDescriptor MaterialContainer::getResourceDesc( MaterialID m )
{
    u32 index = _GetIndex( m );
    return _rdescs[index];
}

}}///

namespace bx{ namespace gfx{

namespace MATERIALS
{
    static const rdi::ResourceBinding bindings[] =
    {
        rdi::ResourceBinding( "MaterialData", rdi::EBindingType::UNIFORM ).Slot( SLOT_MATERIAL_DATA ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "diffuse_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE0 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "specular_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE1 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "roughness_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE2 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "metallic_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE3 ).StageMask( rdi::EStage::PIXEL_MASK ),
    };
    
}///

}}///
