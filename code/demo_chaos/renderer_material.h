#pragma once

#include "renderer_type.h"
#include <util/debug.h>
#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>

namespace bx{ namespace gfx{

struct MaterialContainer
{
    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    id_t                    _index_to_id[eMAX_COUNT] = {};
    rdi::ResourceDescriptor _rdescs[eMAX_COUNT] = {};
    const char*             _names[eMAX_COUNT] = {};

    inline id_t makeId( Material m ) const { return make_id( m.i ); }
    inline Material makeMaterial( id_t id ) const { Material m; m.i = id.hash; return m; }

    bool alive( Material m ) const;

    Material add( const char* name, rdi::ResourceDescriptor rdesc );
    void remove( Material* m );

    Material find( const char* name );

    u32 _GetIndex( Material m );
    rdi::ResourceDescriptor getResourceDesc( Material m );
};
}}///