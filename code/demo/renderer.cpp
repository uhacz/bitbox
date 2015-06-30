#include "renderer.h"
#include <util/debug.h>
#include <util/common.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/id_array.h>
#include <util/pool_allocator.h>

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include "gdi/gdi_context.h"

inline bool operator == ( bxGfx_HMesh a, bxGfx_HMesh b ){
    return a.h == b.h;
}

bxGfx_StreamsDesc::bxGfx_StreamsDesc( int numVerts, int numIndis )
{
    memset( this, 0x00, sizeof( *this ) );
    
    SYS_ASSERT( numVerts > 0 );
    numVertices = numVerts;
    numIndices = numIndis;
    idataType = -1;
}

bxGdiVertexStreamDesc& bxGfx_StreamsDesc::vstream_begin( const void* data /*= 0 */ )
{
    SYS_ASSERT( numVStreams < eMAX_STREAMS );
    
    vdata[numVStreams] = data;
    return vdesc[numVStreams];
}

bxGfx_StreamsDesc& bxGfx_StreamsDesc::vstream_end()
{
    ++numVStreams;
    return *this;
}

bxGfx_StreamsDesc& bxGfx_StreamsDesc::istream_set( bxGdi::EDataType dataType, const void* data /*= 0 */ )
{
    idataType = dataType;
    idata = data;
    return *this;
}



namespace bxGfx
{
    enum
    {
        eMAX_WORLDS = 32,
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
    id_t _Mesh_add( MeshContainer* cnt )
    {
        id_t id = id_table::create( cnt->idTable );
        cnt->rsource[id.index] = 0;
        cnt->fxI[id.index] = 0;
        cnt->instances[id.index].h = 0;
        return id;
    }
    void _Mesh_remove( MeshContainer* cnt, id_t id )
    {
        if ( !id_table::has( cnt->idTable, id ) )
            return;

        cnt->rsource[id.index] = 0;
        cnt->fxI[id.index] = 0;
        cnt->instances[id.index].h = 0;
        id_table::destroy( cnt->idTable, id );
    }

    inline bool _Mesh_valid( MeshContainer* cnt, id_t id )
    {
        return id_table::has( cnt->idTable, id );
    }
    inline bxGdiRenderSource* _Mesh_rsource( MeshContainer* cnt, id_t id )
    {
        SYS_ASSERT( _Mesh_valid( cnt, id ) );
        return cnt->rsource[id.index];
    }
    inline bxGdiShaderFx_Instance* _Mesh_fxI( MeshContainer* cnt, id_t id )
    {
        SYS_ASSERT( _Mesh_valid( cnt, id ) );
        return cnt->fxI[id.index];
    }
    inline bxGfx_HInstanceBuffer _Mesh_instanceBuffer( MeshContainer* cnt, id_t id )
    {
        SYS_ASSERT( _Mesh_valid( cnt, id ) );
        return cnt->instances[id.index];
    }

    struct LightContainer
    {
        
    };

    ////
    ////
    struct InstanceContainer
    {
        struct Data
        {
            Matrix4* pose;
            i32 count;
        };
        id_table_t<bxGfx::eMAX_INSTANCES> idTable;
        Data data[bxGfx::eMAX_INSTANCES];

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
        cnt->_alloc_single.shutdown();
    }

    inline bool _Instance_valid( InstanceContainer* cnt, id_t id )
    {
        return id_table::has( cnt->idTable, id );
    }
    id_t _Instance_add( InstanceContainer* cnt )
    {
        id_t id = id_table::create( cnt->idTable );
        cnt->data[id.index].pose = 0;
        cnt->data[id.index].count = 0;
        return id;
    }
    void _Instance_allocateData( InstanceContainer* cnt, id_t id, int nInstances )
    {
        if ( !_Instance_valid( cnt, id ) )
            return;

        bxAllocator* alloc = (nInstances == 1) ? &cnt->_alloc_single : cnt->_alloc_multi;
        InstanceContainer::Data& e = cnt->data[id.index];

        e.pose = (Matrix4*)alloc->alloc( nInstances * sizeof( Matrix4 ), ALIGNOF( Matrix4 ) );
        e.count = nInstances;
    }
    void _Instance_remove( InstanceContainer* cnt, id_t id )
    {
        if ( !_Instance_valid( cnt, id ) )
            return;

        InstanceContainer::Data& e = cnt->data[id.index];
        bxAllocator* alloc = (e.count == 1) ? &cnt->_alloc_single : cnt->_alloc_multi;
        alloc->free( e.pose );
        e.pose = 0;
        e.count = 0;

        id_table::destroy( cnt->idTable, id );
    }

    inline InstanceContainer::Data _Instance_data( InstanceContainer* cnt, id_t id )
    {
        SYS_ASSERT( _Instance_valid( cnt, id ) );
        return cnt->data[id.index];
    }
    ////
    ////
    struct World
    {
        array_t< bxGfx_HMesh > mesh;

        u32 flag_active : 1;

        World()
            : flag_active(0)
        {}
    };

    int _World_meshFind( World* world, bxGfx_HMesh hmesh )
    {
        return array::find1( array::begin( world->mesh ), array::end( world->mesh ), array::OpEqual<bxGfx_HMesh>( hmesh ) );
    }

    int _World_meshAdd( World* world, bxGfx_HMesh hmesh )
    {
        int index = _World_meshFind( world, hmesh );
        if( index != -1 )
            return index;

        index = array::push_back( world->mesh, hmesh );
        return index;
    }
    void _World_meshRemove( World* world, bxGfx_HMesh hmesh )
    {
        int index = _World_meshFind( world, hmesh );
        if( index == -1 )
        {
            return;
        }

        array::erase_swap( world->mesh, index );
    }

    ////
    ////
    struct ToReleaseEntry
    {
        enum EType
        {
            eINVALID = 0,
            eMESH,
            eINSTANCE_BUFFER,
            eLIGHT,
            eWORLD,
        };
        u32 handle;
        u32 type;
        ToReleaseEntry() : handle( 0 ), type( eINVALID ) {}
        explicit ToReleaseEntry( bxGfx_HMesh h ) : handle( h.h ), type( eMESH ) {}
        explicit ToReleaseEntry( bxGfx_HInstanceBuffer h ) : handle( h.h ), type( eINSTANCE_BUFFER ) {}
        explicit ToReleaseEntry( bxGfx_HWorld h ) : handle( h.h ), type( eWORLD ) {}
    };

    typedef id_array_t< bxGfx::World*, eMAX_WORLDS > WorldContainer;
    typedef array_t< bxGfx::ToReleaseEntry > ToReleaseContainer;
}///

struct bxGfx_Context
{
    bxGfx::MeshContainer mesh;
    bxGfx::InstanceContainer instance;
    bxGfx::WorldContainer world;
    bxGfx::ToReleaseContainer toRelease;

    bxAllocator* _alloc_renderSource;
    bxAllocator* _alloc_shaderFx;
    bxAllocator* _alloc_world;

    bxBenaphore _lock_mesh;
    bxBenaphore _lock_instance;
    bxBenaphore _lock_world;
    bxBenaphore _lock_toRelease;
};
namespace bxGfx
{
    World* _Context_world( bxGfx_Context* ctx, bxGfx_HWorld hworld )
    {
        bxScopeBenaphore lock( ctx->_lock_world );
        if( !id_array::has( ctx->world, make_id( hworld.h ) ) )
        {
            return 0;
        }
        else
        {
            return id_array::get( ctx->world, make_id( hworld.h ) );
        }
    }
}


static bxGfx_Context* __ctx = 0;

namespace bxGfx
{
    void startup()
    {
        SYS_ASSERT( __ctx == 0 );
        __ctx = BX_NEW( bxDefaultAllocator(), bxGfx_Context );
        __ctx->_alloc_renderSource = bxDefaultAllocator();
        __ctx->_alloc_shaderFx = bxDefaultAllocator();
        
        {
            bxPoolAllocator* allocWorld = BX_NEW( bxDefaultAllocator(), bxPoolAllocator );
            allocWorld->startup( sizeof( bxGfx::World ), bxGfx::eMAX_WORLDS, bxDefaultAllocator(), 8 );
            __ctx->_alloc_world = allocWorld;
        }
        
        _Instance_startup( &__ctx->instance );
    }

    void shutdown()
    {
        if ( !__ctx )
            return;

        _Instance_shutdown( &__ctx->instance );
        
        {
            bxPoolAllocator* allocWorld = (bxPoolAllocator*)__ctx->_alloc_world;
            allocWorld->shutdown();
            BX_DELETE0( bxDefaultAllocator(), __ctx->_alloc_world );
        }
        BX_DELETE0( bxDefaultAllocator(), __ctx );
    }

    void frameBegin( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        { /// release pending objects
            bxScopeBenaphore lock( __ctx->_lock_toRelease );
            for ( int i = 0; i < array::size( __ctx->toRelease ); ++i )
            {
                ToReleaseEntry& e = __ctx->toRelease[i];
                switch ( e.type )
                {
                case ToReleaseEntry::eMESH:
                    {
                        bxGfx_HMesh hmesh = { e.handle };
                        bxGdiRenderSource* rsource = _Mesh_rsource( &__ctx->mesh, make_id( hmesh.h ) );
                        bxGdiShaderFx_Instance* fxI = _Mesh_fxI( &__ctx->mesh, make_id( hmesh.h ) );

                        bxGdi::renderSource_releaseAndFree( dev, &rsource, __ctx->_alloc_renderSource );
                        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &fxI, __ctx->_alloc_shaderFx );

                        _Mesh_remove( &__ctx->mesh, make_id( e.handle ) );

                    }break;
                case ToReleaseEntry::eINSTANCE_BUFFER:
                    {
                        _Instance_remove( &__ctx->instance, make_id( e.handle ) );
                    }break;
                case ToReleaseEntry::eLIGHT:
                    {}break;
                default:
                    {
                        bxLogError( "Invalid handle" );
                    }break;
                }
            }
        }

        { /// world garbage collector
            const int nWorlds = id_array::size( __ctx->world );
            bxGfx::World** worlds = id_array::begin( __ctx->world );
            for( int iworld = 0; iworld < nWorlds; ++iworld )
            {
                World* world = worlds[iworld];
                for( int imesh = 0; imesh < array::size( world->mesh ); )
                {
                    bxGfx_HMesh hmesh = world->mesh[imesh];
                    if ( !_Mesh_valid( &__ctx->mesh, make_id( hmesh.h ) ) )
                    {
                        _World_meshRemove( world, hmesh );
                    }
                    else
                    {
                        ++imesh;
                    }
                }
            }
        }
    }

    bxGfx_HMesh mesh_create()
    {
        bxScopeBenaphore lock( __ctx->_lock_mesh );
        id_t id = _Mesh_add( &__ctx->mesh );
        bxGfx_HMesh handle = { id.hash };
        return handle;
    }

    void mesh_release( bxGfx_HMesh* h )
    {
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->toRelease, bxGfx::ToReleaseEntry( h[0] ) );
        bxGfx_HInstanceBuffer hi = _Mesh_instanceBuffer( &__ctx->mesh, make_id( h->h ) );
        if( _Instance_valid( &__ctx->instance, make_id( hi.h ) ) )
        {
            array::push_back( __ctx->toRelease, bxGfx::ToReleaseEntry( hi ) );
        }
        h->h = 0;
    }

    int mesh_setStreams( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, const bxGfx_StreamsDesc& sdesc )
    {
        if( !_Mesh_valid( &__ctx->mesh, make_id( hmesh.h ) ) )
            return -1;

        bxGdiRenderSource* rsource = bxGdi::renderSource_new( sdesc.numVStreams, __ctx->_alloc_renderSource );
        for( int istream = 0; istream < sdesc.numVStreams; ++istream )
        {
            bxGdiVertexBuffer vbuffer = dev->createVertexBuffer( sdesc.vdesc[istream], sdesc.numVertices, sdesc.vdata[istream] );
            bxGdi::renderSource_setVertexBuffer( rsource, vbuffer, istream );
        }

        if( sdesc.numIndices > 0 )
        {
            bxGdiIndexBuffer ibuffer = dev->createIndexBuffer( sdesc.idataType, sdesc.numIndices, sdesc.idata );
            bxGdi::renderSource_setIndexBuffer( rsource, ibuffer );
        }

        const id_t id = make_id( hmesh.h );
        if( __ctx->mesh.rsource[id.index] )
        {
            bxGdi::renderSource_releaseAndFree( dev, &__ctx->mesh.rsource[id.index], __ctx->_alloc_renderSource );
        }
        __ctx->mesh.rsource[id.index] = rsource;
        return 0;
    }

    int mesh_setShader( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* shaderName )
    {
        if( !_Mesh_valid( &__ctx->mesh, make_id( hmesh.h ) ) )
            return -1;

        bxGdiShaderFx_Instance* fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, shaderName, __ctx->_alloc_shaderFx );
        if( !fxI )
            return -1;

        const id_t id = make_id( hmesh.h );
        if( __ctx->mesh.fxI[id.index] )
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__ctx->mesh.fxI[id.index], __ctx->_alloc_shaderFx );
        }

        __ctx->mesh.fxI[id.index] = fxI;
        return 0;
    }

    bxGfx_HInstanceBuffer mesh_createInstanceBuffer( bxGfx_HMesh hmesh, int nInstances )
    {
        bxGfx_HInstanceBuffer handle = { 0 };
        if( !_Mesh_valid( &__ctx->mesh, make_id( hmesh.h ) ) )
            return handle;

        id_t id = _Instance_add( &__ctx->instance );
        _Instance_allocateData( &__ctx->instance, id, nInstances );

        handle.h = id.hash;

        id_t idMesh = { hmesh.h };
        __ctx->mesh.instances[idMesh.index] = handle;

        return handle;
    }

    int instance_get( bxGfx_HInstanceBuffer hinstance, Matrix4* buffer, int bufferSize, int startIndex /*= 0 */ )
    {
        SYS_ASSERT( _Instance_valid( &__ctx->instance, make_id( hinstance.h ) ) );
        int result = 0;

        const InstanceContainer::Data data = _Instance_data( &__ctx->instance, make_id( hinstance.h ) );
        const int endIndex = minOfPair( startIndex + bufferSize, data.count );
        const int count = endIndex - startIndex;
        
        memcpy( buffer, data.pose, count * sizeof( Matrix4 ) );
        
        return count;
    }

    int instance_set( bxGfx_HInstanceBuffer hinstance, const Matrix4* buffer, int bufferSize, int startIndex /*= 0 */ )
    {
        SYS_ASSERT( _Instance_valid( &__ctx->instance, make_id( hinstance.h ) ) );
        
        InstanceContainer::Data data = _Instance_data( &__ctx->instance, make_id( hinstance.h ) );
        const int endIndex = minOfPair( startIndex + bufferSize, data.count );
        const int count = endIndex - startIndex;
        
        memcpy( data.pose, buffer, count * sizeof( Matrix4 ) );

        return count;
    }

    bxGfx_HWorld world_create()
    {
        bxScopeBenaphore lock( __ctx->_lock_world );
        bxGfx::World* world = BX_NEW( __ctx->_alloc_world, bxGfx::World );
        id_t id = id_array::create( __ctx->world, world );

        bxGfx_HWorld result = { id.hash };
        return result;
    }

    void world_release( bxGfx_HWorld* h )
    {
        bxGfx::World* world = _Context_world( __ctx, h[0] );
        if ( !world )
            return;

        world->flag_active = 0;
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->toRelease, bxGfx::ToReleaseEntry( h[0] ) );
    }

    void world_add( bxGfx_HWorld hworld, bxGfx_HMesh hmesh )
    {
        bxGfx::World* world = _Context_world( __ctx, hworld );
        if( !world )
            return;
        
        int index = _World_meshFind( world, hmesh );
        if( index == -1 )
        {
            _World_meshAdd( world, hmesh );
        }
        else
        {
            bxLogWarning( "Mesh already in world" );
        }
    }

    void world_draw( bxGdiContext* ctx, bxGfx_HWorld hworld, const bxGfxCamera& camera )
    {
        bxGfx::World* world = _Context_world( __ctx, hworld );
        if( !world )
            return;

        
    }

}///


