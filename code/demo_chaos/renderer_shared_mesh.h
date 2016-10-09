#pragma once

#include "renderer_type.h"
#include <util/array.h>

namespace bx { namespace gfx {
    
struct SharedMeshContainer
{
    struct Entry
    {
        const char* name = nullptr;
        RenderSource rsource = BX_GFX_NULL_HANDLE;
    };
    array_t< Entry > _entries;

    u32 add( const char* name, RenderSource rs );
    void remove( u32 index );
    u32 find( const char* name );
    RenderSource getRenderSource( u32 index );
};

}}///
