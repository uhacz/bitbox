#include "renderer_shared_mesh.h"
#include <util/debug.h>
#include <util/string_util.h>

namespace bx {namespace gfx {
    u32 SharedMeshContainer::add( const char* name, RenderSource rs )
    {
        u32 index = find( name );
        if( index != UINT32_MAX )
            return index;

        Entry e;
        e.name = string::duplicate( nullptr, name );
        e.rsource = rs;
        return array::push_back( _entries, e );
    }

    void SharedMeshContainer::remove( u32 index )
    {
        SYS_ASSERT( index < array::sizeu( _entries ) );

        Entry& e = _entries[index];
        string::free( (char*)e.name );

        array::erase( _entries, index );
    }

    u32 SharedMeshContainer::find( const char* name )
    {
        for( u32 i = 0; i < array::sizeu( _entries ); ++i )
        {
            if( string::equal( name, _entries[i].name ) )
                return i;
        }
        return UINT32_MAX;
    }

    RenderSource SharedMeshContainer::getRenderSource( u32 index )
    {
        SYS_ASSERT( index < array::sizeu( _entries ) );
        return _entries[index].rsource;
    }
}}///