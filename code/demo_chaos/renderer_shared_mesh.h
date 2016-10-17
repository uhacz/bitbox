#pragma once

#include "renderer_type.h"
#include <util/array.h>

namespace bx { namespace gfx {
    
struct SharedMeshContainer
{
    struct Entry
    {
        const char* name = nullptr;
        rdi::RenderSource rsource = BX_RDI_NULL_HANDLE;
    };
    array_t< Entry > _entries;

    u32 add( const char* name, rdi::RenderSource rs );
    void remove( u32 index );
    u32 find( const char* name );
    rdi::RenderSource getRenderSource( u32 index );
};

}}///
