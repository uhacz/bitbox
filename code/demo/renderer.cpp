#include "renderer.h"
#include <util/debug.h>
#include <util/id_table.h>
#include <util/pool_allocator.h>
#include <util/thread/mutex.h>

namespace bxGfx
{
    enum
    {
        eMAX_MESHES = 1024,
        eMAX_INSTANCES = 1024,
    };

    ////
    ////
    struct MeshContainer
    {
        id_table_t<bxGfx::eMAX_MESHES> idTable;
        bxGdiRenderSource* rsource[bxGfx::eMAX_MESHES];
        bxGdiShaderFx_Instance* fxI[bxGfx::eMAX_MESHES];
        bxGfx_HInstanceBuffer instances[bxGfx::eMAX_MESHES];
    };
    id_t mesh_add( MeshContainer* cnt )
    {
        id_t id = id_table::create( cnt->idTable );
        cnt->rsource[id.index] = 0;
        cnt->fxI[id.index] = 0;
        cnt->instances[id.index].h = 0;
        return id;
    }
    void mesh_remove( MeshContainer* cnt, id_t id )
    {
        if ( !id_table::has( cnt->idTable, id ) )
            return;

        cnt->rsource[id.index] = 0;
        cnt->fxI[id.index] = 0;
        cnt->instances[id.index].h = 0;
        id_table::destroy( cnt->idTable, id );
    }

    inline bool mesh_valid( MeshContainer* cnt, id_t id )
    {
        return id_table::has( cnt->idTable, id );
    }
    inline bxGdiRenderSource* mesh_rsource( MeshContainer* cnt, id_t id )
    {
        SYS_ASSERT( mesh_valid( cnt, id ) );
        return cnt->rsource[id.index];
    }
    inline bxGdiShaderFx_Instance* mesh_fxI( MeshContainer* cnt, id_t id )
    {
        SYS_ASSERT( mesh_valid( cnt, id ) );
        return cnt->fxI[id.index];
    }

    struct LightContainer
    {
        
    };

    ////
    ////
    struct InstanceContainer
    {
        struct Entry
        {
            Matrix4* pose;
            i32 count;
        };
        id_table_t<bxGfx::eMAX_INSTANCES> idTable;
        Entry entries[bxGfx::eMAX_INSTANCES];

        bxPoolAllocator _alloc_single;
        bxAllocator* _alloc_multi;

        InstanceContainer()
            : _alloc_multi(0)
        {}

    };
    void _Instance_startup( InstanceContainer* cnt )
    {
        cnt->_alloc_single.startup( sizeof( Matrix4 ), bxGfx::eMAX_INSTANCES, bxDefaultAllocator(), ALIGNOF( Matrix4 ) );
        cnt->_alloc_multi = bxDefaultAllocator();
    }
    void _Instance_shutdown( InstanceContainer* cnt )
    {
        cnt->_alloc_multi = 0;
        cnt->_alloc_single.shutdown( bxDefaultAllocator() );
    }

    inline bool instance_valid( InstanceContainer* cnt, id_t id )
    {
        return id_table::has( cnt->idTable, id );
    }
    id_t instance_add( InstanceContainer* cnt )
    {
        id_t id = id_table::create( cnt->idTable );
        cnt->entries[id.index].pose = 0;
        cnt->entries[id.index].count = 0;
        return id;
    }
    void instance_allocateData( InstanceContainer* cnt, id_t id, int nInstances )
    {
        if ( !instance_valid( cnt, id ) )
            return;

        bxAllocator* alloc = (nInstances == 1) ? &cnt->_alloc_single : cnt->_alloc_multi;
        InstanceContainer::Entry& e = cnt->entries[id.index];

        e.pose = (Matrix4*)alloc->alloc( nInstances * sizeof( Matrix4 ), ALIGNOF( Matrix4 ) );
        e.count = nInstances;
    }
    void instance_remove( InstanceContainer* cnt, id_t id )
    {
        if ( !instance_valid( cnt, id ) )
            return;

        InstanceContainer::Entry& e = cnt->entries[id.index];
        bxAllocator* alloc = (e.count == 1) ? &cnt->_alloc_single : cnt->_alloc_multi;
        alloc->free( e.pose );
        e.pose = 0;
        e.count = 0;

        id_table::destroy( cnt->idTable, id );
    }



}///
struct bxGfx_World;
struct bxGfx_Context
{
    bxGfx::MeshContainer mesh;
    bxGfx::InstanceContainer instance;
    array_t< bxGfx_World* > world;

    bxBenaphore _lock_mesh;
    bxBenaphore _lock_instance;
    bxBenaphore _lock_world;
};

struct bxGfx_World
{
    array_t< bxGfx_HMesh > mesh;
};

static bxGfx_Context* __ctx = 0;

namespace bxGfx
{
    void startup()
    {
        SYS_ASSERT( __ctx == 0 );
        __ctx = BX_NEW( bxDefaultAllocator(), bxGfx_Context );
        _Instance_startup( &__ctx->instance );
    }

    void shutdown()
    {
        if ( !__ctx )
            return;

        _Instance_shutdown( &__ctx->instance );
        BX_DELETE0( bxDefaultAllocator(), __ctx );
    }

    bxGfx_HMesh mesh_create()
    {
        bxScopeBenaphore lock( __ctx->_lock_mesh );
        id_t id = mesh_add( &__ctx->mesh );
        bxGfx_HMesh handle = { id.hash };
        return handle;
    }

    void mesh_release( bxGfx_HMesh* h )
    {

    }

}///