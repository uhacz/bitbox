#include "renderer_shared_mesh.h"
#include <util/debug.h>
#include <util/string_util.h>
#include <util/hash.h>
#include <util/hashmap.h>
#include <util/tag.h>
//#include <resource_manager/resource_manager.h>


//namespace bx {namespace gfx {
//    void SharedMeshContainer::add( const char* name, rdi::RenderSource rs )
//    {
//        u32 index = find( name );
//        if( index != UINT32_MAX )
//            return;
//
//        Entry e;
//        e.name = string::duplicate( nullptr, name );
//        e.rsource = rs;
//        array::push_back( _entries, e );
//    }
//
//    void SharedMeshContainer::remove( const char* name, rdi::RenderSource* rs )
//    {
//        u32 index = find( name );
//        SYS_ASSERT( index < array::sizeu( _entries ) );
//
//        Entry& e = _entries[index];
//        rs[0] = e.rsource;
//        string::free( (char*)e.name );
//
//        array::erase( _entries, index );
//    }
//
//    u32 SharedMeshContainer::find( const char* name )
//    {
//        for( u32 i = 0; i < array::sizeu( _entries ); ++i )
//        {
//            if( string::equal( name, _entries[i].name ) )
//                return i;
//        }
//        return UINT32_MAX;
//    }
//
//    rdi::RenderSource SharedMeshContainer::get( u32 index )
//    {
//        SYS_ASSERT( index < array::sizeu( _entries ) );
//        return _entries[index].rsource;
//    }
//}
//}///


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace bx { namespace gfx {
    
namespace mesh_manager_internal
{
    static const u32 HASH_SEED = bxTag32( "MESH" );
    inline u32 GenerateNameHash( const char* name )
    {
        return murmur3_hash32( name, (u32)strlen( name ), HASH_SEED );
    }
    inline u64 CreateMapKey( u32 hashedName )
    {
        return u64( HASH_SEED ) << 32 | u64( hashedName );
    }
}///

MeshHandle MeshManager::Add( const char* name )
{
    MeshHandle foundId = Find( name );
    if( IsValid( foundId ) )
    {
        bxLogWarning( "Mesh with name '%s' already exists", name );
        return MeshHandle();
    }

    //ResourceID resource_id = ResourceManager::createResourceID( name, "mesh" );

    const u32 name_hash = mesh_manager_internal::GenerateNameHash( name );
    const u64 map_key = mesh_manager_internal::CreateMapKey( name_hash );
    SYS_ASSERT( hashmap::lookup( _lookup_map, map_key ) == nullptr );

    _lock.lock();
    id_t id = id_table::create( _id );
    hashmap_t::cell_t* cell = hashmap::insert( _lookup_map, map_key );
    _lock.unlock();

    cell->value = id.hash;
    _name_hash[id.index] = name_hash;
    //GResourceManager()->insertResource( resource_id, ResourcePtr( id.hash ) );
    return MakeHandle( id );
}

void MeshManager::Remove( MeshHandle* h )
{
    id_t id = { h->i };
    if( !id_table::has( _id, id ) )
        return;
    
    const u32 name_hash = _name_hash[id.index];
    const u64 map_key = mesh_manager_internal::CreateMapKey( name_hash );
    
    
    //GResourceManager()->releaseResource( ResourcePtr( id.hash ) );
    
    _lock.lock();

    hashmap_t::cell_t* cell = hashmap::lookup( _lookup_map, map_key );
    SYS_ASSERT( cell != nullptr );
    hashmap::erase( _lookup_map, cell );

    id_table::destroy( _id, id );

    _lock.unlock();
}

void MeshManager::RemoveByName( const char* name, rdi::RenderSource* rsource )
{
    MeshHandle hmesh = Find( name );
    if( Alive( hmesh )  && rsource )
    {
        rsource[0] = this->RenderSource( hmesh );
    }
    Remove( &hmesh );
}

void MeshManager::SetRenderSource( MeshHandle h, rdi::RenderSource rsource )
{
    if( !Alive( h ) )
        return;

    _rsource[MakeId( h ).index] = rsource;
}

void MeshManager::SetLocalAABB( MeshHandle h, const bxAABB& aabb )
{
    if( !Alive( h ) )
        return;

    _local_aabb[MakeId( h ).index] = aabb;
}

MeshHandle MeshManager::Find( const char* name )
{
    //ResourceID resource_id = ResourceManager::createResourceID( name, "mesh" );
    //ResourcePtr resource_ptr = GResourceManager()->acquireResource( resource_id );

    using namespace mesh_manager_internal;
    const u64 map_key = CreateMapKey( GenerateNameHash( name ) );

    MeshHandle h = {};

    _lock.lock();
    hashmap_t::cell_t* cell = hashmap::lookup( _lookup_map, map_key );
    if( cell )
        h.i = (u32)cell->value;
    _lock.unlock();

    //MeshHandle h = { (u32)resource_ptr };
    return h;
}

static MeshManager* g_mesh_manager = nullptr;
void MeshManager::_StartUp()
{
    SYS_ASSERT( g_mesh_manager == nullptr );
    g_mesh_manager = BX_NEW( bxDefaultAllocator(), MeshManager );
}

void MeshManager::_ShutDown()
{
    SYS_ASSERT( g_mesh_manager != nullptr );
    BX_DELETE0( bxDefaultAllocator(), g_mesh_manager );
}

MeshManager* GMeshManager()
{
    return g_mesh_manager;
}

}
}///
