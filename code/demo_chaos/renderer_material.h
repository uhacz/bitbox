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

    inline id_t makeId( MaterialID m ) const { return make_id( m.i ); }
    inline MaterialID makeMaterial( id_t id ) const { MaterialID m; m.i = id.hash; return m; }

    bool alive( MaterialID m ) const;

    MaterialID add( const char* name, rdi::ResourceDescriptor rdesc );
    void remove( MaterialID* m );

    MaterialID find( const char* name );

    u32 _GetIndex( MaterialID m );
    rdi::ResourceDescriptor getResourceDesc( MaterialID m );
};
}}///