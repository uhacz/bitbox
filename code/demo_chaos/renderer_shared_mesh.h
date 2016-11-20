#pragma once

#include "renderer_type.h"
#include <util/array.h>
//
//namespace bx { namespace gfx {
//    
//struct SharedMeshContainer
//{
//    struct Entry
//    {
//        const char* name = nullptr;
//        rdi::RenderSource rsource = BX_RDI_NULL_HANDLE;
//    };
//    array_t< Entry > _entries;
//
//    void add( const char* name, rdi::RenderSource rs );
//    void remove( const char* name, rdi::RenderSource* rs );
//    
//    inline rdi::RenderSource query( const char* name )
//    {
//        u32 index = find( name );
//        return ( index != UINT32_MAX ) ? get( index ) : BX_RDI_NULL_HANDLE;
//    }
//
//private:
//    u32 find( const char* name );
//    rdi::RenderSource get( u32 index );
//};
//
//}}///

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#include <util/bbox.h>
#include <util/thread/mutex.h>
#include <util/debug.h>
#include <util/id_table.h>
namespace bx { namespace gfx {



class MeshManager
{
public:
    MeshHandle Add( const char* name );
    void Remove( MeshHandle* h );
    void RemoveByName( const char* name, rdi::RenderSource* rsource );

    bool Alive( MeshHandle h ) const { return id_table::has( _id, MakeId( h ) ); }

    void SetRenderSource( MeshHandle h, rdi::RenderSource rsource );
    rdi::RenderSource RenderSource( MeshHandle h ) const
    {
        SYS_ASSERT( Alive( h ) );
        return _rsource[MakeId( h ).index];
    }

    void SetLocalAABB( MeshHandle h, const bxAABB& aabb );
    bxAABB LocalAABB( MeshHandle h ) const
    {
        SYS_ASSERT( Alive( h ) );
        return _local_aabb[MakeId( h ).index];
    }
    
    MeshHandle CreateFromFile( const char* fileName, const char* name );
    MeshHandle Find( const char* name );

    //////////////////////////////////////////////////////////////////////////
    static void _StartUp();
    static void _ShutDown();

private:
    static inline id_t MakeId( MeshHandle h )
    {
        id_t id = { h.i };
        return id;
    }
    static inline MeshHandle MakeHandle( id_t id )
    {
        MeshHandle h = { id.hash };
        return h;
    }

    static const u8 MAX_MESHES = 128;
    id_table_t<MAX_MESHES> _id;
    rdi::RenderSource _rsource[MAX_MESHES] = {};
    bxAABB _local_aabb[MAX_MESHES] = {};
    u32 _name_hash[MAX_MESHES] = {};
    
    hashmap_t _lookup_map;

    bxBenaphore _lock;
};
MeshManager* GMeshManager();

}}///
