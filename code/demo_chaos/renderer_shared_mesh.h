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

    void add( const char* name, rdi::RenderSource rs );
    void remove( u32 index );
    
    inline rdi::RenderSource query( const char* name )
    {
        u32 index = find( name );
        return ( index != UINT32_MAX ) ? get( index ) : BX_RDI_NULL_HANDLE;
    }

private:
    u32 find( const char* name );
    rdi::RenderSource get( u32 index );
};

}}///
