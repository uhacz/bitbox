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
#include <gdi/gdi_context.h>

#include <gfx/gfx_type.h>
#include "util/buffer_utils.h"
#include "util/random.h"
#include "util/time.h"

inline bool operator == ( bxGfx_HMesh a, bxGfx_HMesh b ){
    return a.h == b.h;
}
inline bool operator == (bxGfx_HInstanceBuffer a, bxGfx_HInstanceBuffer b){
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

bxGdiVertexStreamDesc& bxGfx_StreamsDesc::vstreamBegin( const void* data /*= 0 */ )
{
    SYS_ASSERT( numVStreams < eMAX_STREAMS );
    
    vdata[numVStreams] = data;
    return vdesc[numVStreams];
}

bxGfx_StreamsDesc& bxGfx_StreamsDesc::vstreamEnd()
{
    ++numVStreams;
    return *this;
}

bxGfx_StreamsDesc& bxGfx_StreamsDesc::istreamSet( bxGdi::EDataType dataType, const void* data /*= 0 */ )
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
        eNUM_SHADOW_CASCADES = 4,
    };
    union MeshFlags
    {
        u8 all;
        struct  
        {
            u8 own_rsource : 1;
            u8 own_fxI : 1;
        };
    };
    ////
    ////
    struct MeshContainer
    {
        id_table_t<bxGfx::eMAX_MESHES> _idTable;
        bxGdiRenderSource* _rsource[bxGfx::eMAX_MESHES];
        bxGdiShaderFx_Instance* _fxI[bxGfx::eMAX_MESHES];
        MeshFlags _flags[bxGfx::eMAX_MESHES];

        id_t add()
        {
            id_t id = id_table::create( _idTable );
            _rsource[id.index] = 0;
            _fxI[id.index] = 0;
            _flags[id.index].all = 0;
            return id;
        }
        void remove( id_t id )
        {
            if ( !id_table::has( _idTable, id ) )
                return;

            _rsource[id.index] = 0;
            _fxI[id.index] = 0;
            _flags[id.index].all = 0;
            id_table::destroy( _idTable, id );
        }

        inline bool has( id_t id )
        {
            return id_table::has( _idTable, id );
        }
        inline bxGdiRenderSource* rsource( id_t id )
        {
            SYS_ASSERT( has( id ) );
            return _rsource[id.index];
        }
        inline bxGdiShaderFx_Instance* fxI( id_t id )
        {
            SYS_ASSERT( has( id ) );
            return _fxI[id.index];
        }
    };
    
    inline int _Mesh_index( bxGfx_HMesh hmesh )
    {
        return make_id( hmesh.h ).index;
    }

    ////
    ////
    struct InstanceContainer
    {
        struct Data
        {
            Matrix4* pose;
            i32 count;
        };
        id_table_t<bxGfx::eMAX_INSTANCES> _idTable;
        Data _data[bxGfx::eMAX_INSTANCES];

        bxPoolAllocator _alloc_single;
        bxAllocator* _alloc_multi;

        InstanceContainer()
            : _alloc_multi(0)
        {}

        void startup()
        {
            _alloc_single.startup( sizeof( Matrix4 ), bxGfx::eMAX_INSTANCES, bxDefaultAllocator(), ALIGNOF( Matrix4 ) );
            _alloc_multi = bxDefaultAllocator();
        }
        void shutdown()
        {
            _alloc_multi = 0;
            _alloc_single.shutdown();
        }

        inline bool has( id_t id )
        {
            return id_table::has( _idTable, id );
        }
        id_t add()
        {
            id_t id = id_table::create( _idTable );
            _data[id.index].pose = 0;
            _data[id.index].count = 0;
            return id;
        }
        void allocateData( id_t id, int nInstances )
        {
            if ( !has( id ) )
                return;

            bxAllocator* alloc = (nInstances == 1) ? &_alloc_single : _alloc_multi;
            InstanceContainer::Data& e = _data[id.index];

            e.pose = (Matrix4*)alloc->alloc( nInstances * sizeof( Matrix4 ), ALIGNOF( Matrix4 ) );
            e.count = nInstances;
        }
        void remove( id_t id )
        {
            if ( !has( id ) )
                return;

            InstanceContainer::Data& e = _data[id.index];
            bxAllocator* alloc = (e.count == 1) ? &_alloc_single : _alloc_multi;
            alloc->free( e.pose );
            e.pose = 0;
            e.count = 0;

            id_table::destroy( _idTable, id );
        }

        inline InstanceContainer::Data data( id_t id )
        {
            SYS_ASSERT( has( id ) );
            return _data[id.index];
        }

    };


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
            eMESH_INSTANCE,
        };
        union
        {
            u32 handle32;
            u64 handle64;
            uptr handlePtr;
        };
        u32 type;
        ToReleaseEntry() : handle64( 0 ), type( eINVALID ) {}
        explicit ToReleaseEntry( bxGfx_HMesh h ) : handle32( h.h ), type( eMESH ) {}
        explicit ToReleaseEntry( bxGfx_HInstanceBuffer h ) : handle32( h.h ), type( eINSTANCE_BUFFER ) {}
        explicit ToReleaseEntry( bxGfx_World* h ) : handlePtr( (uptr)h ), type( eWORLD ) {}
        explicit ToReleaseEntry( bxGfx_HMeshInstance h ): handle64( h.h ), type( eMESH_INSTANCE ) {}
    };

    struct WorldContainer
    {
        //id_array_t< eMAX_WORLDS > idArray;
        array_t< bxGfx_World* > world;

        int add( bxGfx_World* w )
        {
            //id_t id = id_array::create( idArray );
            int index = array::push_back( world, w );
            //SYS_ASSERT( id_array::index( idArray, id ) == index );

            return index;
        }
        
        void remove( /*id_t id*/int index )
        {
            //if( !id_array::has( idArray, id ) )
            //    return;
            if( index == -1 || index > array::size( world ) )
                return;

            array::erase_swap( world, index );

            //int last = id_array::size( idArray ) - 1;
            //int index = id_array::index( idArray, id );
            //id_array::destroy( idArray, id );
            
            //world[index] = world[last];
            //array::pop_back( world );
        }

        int indexGet( bxGfx_World* ptr )
        {
            return array::find1( array::begin( world ), array::end( world ), array::OpEqual<bxGfx_World*>( ptr ) );
        }

        //World* get( id_t id )
        //{
        //    return ( id_array::has( idArray, id ) ) ? world[id_array::index( idArray, id )] : 0;
        //}

        WorldContainer()
        {
            array::reserve( world, eMAX_WORLDS );
        }
    };
    typedef array_t< bxGfx::ToReleaseEntry > ToReleaseContainer;
}///

namespace bxGfx
{
    inline bxGfx_HMeshInstance makeMeshInstance( bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance )
    {
        bxGfx_HMeshInstance result = { 0 };
        result.h = u64( hmesh.h ) << 32 | u64( hinstance.h );
        return result;
    }

    inline bxGfx_HMesh hMeshGet( bxGfx_HMeshInstance hMeshInstance )
    {
        bxGfx_HMesh result = { 0 };
        result.h = u32( hMeshInstance.h >> 32 );
        return result;
    }
    inline bxGfx_HInstanceBuffer hInstanceBufferGet( bxGfx_HMeshInstance hMeshInstance )
    {
        bxGfx_HInstanceBuffer result = { 0 };
        result.h = u32( hMeshInstance.h & 0xFFFFFFFF );
        return result;
    }
}



struct bxGfx_Context
{
    bxGfx::MeshContainer _mesh;
    bxGfx::InstanceContainer _instance;
    bxGfx::WorldContainer _world;
    bxGfx::ToReleaseContainer _toRelease;

    hashmap_t _map_meshInstanceToWorld;

    array_t<u32> _instanceOffset;
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceOffset;
    bxGdiBuffer _buffer_instanceWorld;
    bxGdiBuffer _buffer_instanceWorldIT;
    u32 _maxInstances;

    bxAllocator* _alloc_renderSource;
    bxAllocator* _alloc_shaderFx;
    bxAllocator* _alloc_world;

    bxBenaphore _lock_mesh;
    bxBenaphore _lock_instance;
    bxBenaphore _lock_world;
    bxBenaphore _lock_toRelease;

    u32 _flag_resourceReleased : 1;

    ////
    ////
    //bxGfx::World* world( bxGfx_HWorld hworld )
    //{
    //    bxScopeBenaphore lock( _lock_world );
    //    return _world.get( make_id( hworld.h ) );
    //}
    bxGfx_World* lookupWorld( bxGfx_HMeshInstance hmeshi )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( _map_meshInstanceToWorld, hmeshi.h );
        bxGfx_World* result = 
        {
            (cell) ? (bxGfx_World*)cell->value : 0,
        };
        return  result;
    }
};
static bxGfx_Context* __ctx = 0;


namespace bxGfx
{

union SortKeyColor
{
    u64 hash;
    struct
    {
        u64 mesh : 24;
        u64 shader : 32;
        u64 layer : 8;
    };
};

union SortKeyDepth
{
    u16 hash;
    u16 depth;
};
union SortKeyShadow
{
    u32 hash;
    struct  
    {
        u16 depth;
        u16 cascade;
    };
};


}///

typedef bxGdiSortList< bxGfx::SortKeyColor > bxGfx_SortListColor;
typedef bxGdiSortList< bxGfx::SortKeyDepth > bxGfx_SortListDepth;

struct bxGfx_World
{
    struct Data
    {
        bxGfx_HMesh* mesh;
        bxGfx_HInstanceBuffer* instance;
        bxAABB* localAABB;

        void* memoryHandle;
        i32 size;
        i32 capacity;
    };
    Data _data;
    bxAllocator* _alloc_data;

    bxBenaphore _lock_toRemove;
    array_t< i32 > _toRemove;

    bxGfx_SortListColor* _sList_color;
    bxGfx_SortListDepth* _sList_depth;
    bxGfx_SortListDepth* _sList_shadow;

    u32 flag_active : 1;

    bxGfx_World()
        : _sList_color( nullptr )
        , _sList_depth( nullptr )
        , _sList_shadow( nullptr )
        , flag_active( 0 )
        
    {
        memset( &_data, 0x00, sizeof( bxGfx_World::Data ) );
        _alloc_data = bxDefaultAllocator();
    }

    void shutdown()
    {
        BX_FREE0( _alloc_data, _data.memoryHandle );

        bxGdi::sortList_delete( &_sList_shadow );
        bxGdi::sortList_delete( &_sList_depth );
        bxGdi::sortList_delete( &_sList_color );

        memset( &_data, 0x00, sizeof( bxGfx_World::Data ) );
    }

    static void allocateData( bxGfx_World::Data* data, int newcap, bxAllocator* alloc )
    {
        SYS_ASSERT( newcap > data->size );

        int memSize = 0;
        //memSize += newcap * sizeof( *data->entity );
        memSize += newcap * sizeof( *data->mesh );
        memSize += newcap * sizeof( *data->instance );
        memSize += newcap * sizeof( *data->localAABB );

        void* memory = BX_MALLOC( alloc, memSize, ALIGNOF( bxEntity_Id ) );
        memset( memory, 0x00, memSize );

        bxGfx_World::Data newData;
        memset( &newData, 0x00, sizeof( bxGfx_World::Data ) );
        newData.size = data->size;
        newData.capacity = newcap;
        newData.memoryHandle = memory;

        bxBufferChunker chunker( memory, memSize );
        //newData.entity = chunker.add< bxEntity_Id >( newcap );
        newData.mesh = chunker.add< bxGfx_HMesh >( newcap );
        newData.instance = chunker.add< bxGfx_HInstanceBuffer >( newcap );
        newData.localAABB = chunker.add< bxAABB >( newcap );
        chunker.check();

        if( data->size )
        {
            //BX_CONTAINER_COPY_DATA( &newData, data, entity );
            BX_CONTAINER_COPY_DATA( &newData, data, mesh );
            BX_CONTAINER_COPY_DATA( &newData, data, instance );
            BX_CONTAINER_COPY_DATA( &newData, data, localAABB );
        }

        BX_FREE0( alloc, data->memoryHandle );

        data[0] = newData;
    }

    inline int meshFind( bxGfx_HMesh hmesh )
    {
        return array::find1( _data.mesh, _data.mesh + _data.size, array::OpEqual<bxGfx_HMesh>( hmesh ) );
    }
    inline int instanceFind( bxGfx_HInstanceBuffer hinstance )
    {
        return array::find1( _data.instance, _data.instance + _data.size, array::OpEqual<bxGfx_HInstanceBuffer>( hinstance ) );
    }

    //int _World_meshLookup( World* world, bxEntity_Id eid )
    //{
    //    hashmap_t::cell_t* cell = hashmap::lookup( world->_entityMap, eid.hash );
    //    return ( cell ) ? (int)cell->value : -1;
    //}

    bxGfx_HMeshInstance add( bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance )
    {
        int index = meshFind( hmesh );
        if( index != -1 )
            return index;

        if( _data.size + 1 > _data.capacity )
        {
            const int newcap = _data.capacity * 2 + 8;
            allocateData( &_data, newcap, _alloc_data );
        }

        aaa tutej

        bxGfx_World::Data& data = _data;
        index = data.size++;

        data.mesh[index] = hmesh;
        data.instance[index] = hinstance;

        return index;
    }

    void removeByIndex( int index )
    {
        if( index == -1 || index > _data.size )
            return;

        bxGfx_World::Data& data = _data;
        if( data.size == 0 )
            return;

        const int lastIndex = --data.size;

        //{
        //    bxEntity_Id eid = data.entity[index];
        //    hashmap::eraseByKey( world->_entityMap, eid.hash );
        //}

        //data.entity[index] = data.entity[lastIndex];
        data.mesh[index] = data.mesh[lastIndex];
        data.instance[index] = data.instance[lastIndex];

        //{
        //    bxEntity_Id eid = data.entity[index];
        //    hashmap_t::cell_t* mapCell = hashmap::lookup( world->_entityMap, eid.hash );
        //    SYS_ASSERT( mapCell != 0 );
        //    mapCell->value = size_t( index );
        //}
    }

    void remove( bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance )
    {
        int index0 = meshFind( hmesh );
        int index1 = instanceFind( hinstance );

        if( index0 == index1 )
        {
            removeByIndex( index0 );
        }
    }

    void gc( bool checkResources )
    {
        bxGfx_World::Data& data = _data;
        if( checkResources )
        {
            for( int i = 0; i < data.size; ++i )
            {
                if( !__ctx->_mesh.has( make_id( data.mesh[i].h ) ) )
                {
                    array::push_back( _toRemove, i );
                }
            }
        }

        for( int i = 0; i < array::size( _toRemove ); ++i )
        {
            int index = _toRemove[i];
            removeByIndex( index );
        }
        array::clear( _toRemove );
    }
};
namespace bxGfx
{
    void startup( bxGdiDeviceBackend* dev )
    {
        SYS_ASSERT( __ctx == 0 );
        __ctx = BX_NEW( bxDefaultAllocator(), bxGfx_Context );
        __ctx->_alloc_renderSource = bxDefaultAllocator();
        __ctx->_alloc_shaderFx = bxDefaultAllocator();
        __ctx->_flag_resourceReleased = 0;

        {
            bxPoolAllocator* allocWorld = BX_NEW( bxDefaultAllocator(), bxPoolAllocator );
            allocWorld->startup( sizeof( bxGfx_World ), bxGfx::eMAX_WORLDS, bxDefaultAllocator(), 8 );
            __ctx->_alloc_world = allocWorld;
        }
        
        __ctx->_instance.startup();

        { /// gpu buffers
            __ctx->_cbuffer_frameData = dev->createConstantBuffer( sizeof( bxGfx::FrameData ) );
            __ctx->_cbuffer_instanceOffset = dev->createConstantBuffer( sizeof( u32 ) );
            
            const int maxInstances = eMAX_INSTANCES * 2;
            const int numElements = maxInstances * 3; /// 3 * row
            __ctx->_buffer_instanceWorld   = dev->createBuffer( numElements, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            __ctx->_buffer_instanceWorldIT = dev->createBuffer( numElements, bxGdiFormat( bxGdi::eTYPE_FLOAT, 3 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            __ctx->_maxInstances = maxInstances;
            array::reserve( __ctx->_instanceOffset, maxInstances );
        }
    }

    static void _Context_manageResources( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        for( int i = 0; i < array::size( __ctx->_toRelease ); ++i )
        {
            ToReleaseEntry& e = __ctx->_toRelease[i];
            switch( e.type )
            {
            case ToReleaseEntry::eMESH:
                {
                    bxGfx_HMesh hmesh = { e.handle32 };
                    const int index = _Mesh_index( hmesh );
                    MeshFlags flags = __ctx->_mesh._flags[index];

                    if( flags.own_rsource )
                    {
                        bxGdiRenderSource* rsource = __ctx->_mesh.rsource( make_id( hmesh.h ) );
                        bxGdi::renderSource_releaseAndFree( dev, &rsource, __ctx->_alloc_renderSource );
                    }

                    if( flags.own_fxI )
                    {
                        bxGdiShaderFx_Instance* fxI = __ctx->_mesh.fxI( make_id( hmesh.h ) );
                        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &fxI, __ctx->_alloc_shaderFx );
                    }

                    __ctx->_mesh.remove( make_id( e.handle32 ) );
                }break;
            case ToReleaseEntry::eINSTANCE_BUFFER:
                {
                    __ctx->_instance.remove( make_id( e.handle32 ) );
                }break;
            case ToReleaseEntry::eLIGHT:
                {}break;
            case ToReleaseEntry::eMESH_INSTANCE:
                {
                    bxGfx_HMeshInstance hmeshi = { e.handle64 };
                    //bxGfx_HWorld hworld = __ctx->lookupWorld( hmeshi );
                    bxGfx_World* world = __ctx->lookupWorld( hmeshi ); // __ctx->world( hworld );
                    if( world )
                    {
                        bxGfx_HMesh hmesh = hMeshGet( hmeshi );
                        bxGfx_HInstanceBuffer hinstane = hInstanceBufferGet( hmeshi );

                        world->remove( hmesh, hinstane );
                        hashmap::eraseByKey( __ctx->_map_meshInstanceToWorld, hmeshi.h );
                    }
                }break;
            case ToReleaseEntry::eWORLD:
                {
                    //id_t id = { e.handle32 };
                    bxGfx_World* world = (bxGfx_World*)e.handlePtr;// __ctx->_world.get( id );
                    int index = __ctx->_world.indexGet( world );
                    __ctx->_world.remove( index );
                    
                    world->shutdown();
                    BX_DELETE0( __ctx->_alloc_world, world );
                }break;
            default:
                {
                    bxLogError( "Invalid handle" );
                }break;
            }

            __ctx->_flag_resourceReleased = 1;
        }

        array::clear( __ctx->_toRelease );
    }
    static void _Context_worldsGC()
    {
        const bool checkResources = __ctx->_flag_resourceReleased == 1;
        const int nWorlds = array::size( __ctx->_world.world );
        bxGfx_World** worlds = array::begin( __ctx->_world.world );
        for( int iworld = 0; iworld < nWorlds; ++iworld )
        {
            bxGfx_World* world = worlds[iworld];
            bxScopeBenaphore lock( world->_lock_toRemove );
            world->gc( checkResources );
        }
    }

    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        if ( !__ctx )
            return;
        
        _Context_worldsGC();
        _Context_manageResources( dev, resourceManager );

        {
            dev->releaseBuffer( &__ctx->_buffer_instanceWorldIT );
            dev->releaseBuffer( &__ctx->_buffer_instanceWorld );
            dev->releaseBuffer( &__ctx->_cbuffer_instanceOffset );
            dev->releaseBuffer( &__ctx->_cbuffer_frameData );
        }

        __ctx->_instance.shutdown();
        
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
            _Context_manageResources( dev, resourceManager );
        }

        { /// world garbage collector
            _Context_worldsGC();
        }

        __ctx->_flag_resourceReleased = 0;
    }

    bxGfx_HMesh meshCreate()
    {
        bxScopeBenaphore lock( __ctx->_lock_mesh );
        id_t id = __ctx->_mesh.add();
        bxGfx_HMesh handle = { id.hash };
        return handle;
    }

    void meshRelease( bxGfx_HMesh* h )
    {
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( h[0] ) );
        h->h = 0;
    }

    int meshStreamsSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, const bxGfx_StreamsDesc& sdesc )
    {
        if( !__ctx->_mesh.has( make_id( hmesh.h ) ) )
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

        const int index = _Mesh_index( hmesh );
        MeshFlags& flags = __ctx->_mesh._flags[index];

        if( flags.own_rsource )
        {
            bxGdi::renderSource_releaseAndFree( dev, &__ctx->_mesh._rsource[index], __ctx->_alloc_renderSource );
        }

        flags.own_rsource = 1;
        __ctx->_mesh._rsource[index] = rsource;
        return 0;
    }

    int meshStreamsSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource )
    {
        if ( !__ctx->_mesh.has( make_id( hmesh.h ) ) )
            return -1;

        const int index = _Mesh_index( hmesh );
        MeshFlags& flags = __ctx->_mesh._flags[index];
        if ( flags.own_rsource )
        {
            bxGdi::renderSource_releaseAndFree( dev, &__ctx->_mesh._rsource[index], __ctx->_alloc_renderSource );
        }
        flags.own_rsource = 0;
        __ctx->_mesh._rsource[index] = rsource;
        return 0;
    }

    bxGdiShaderFx_Instance* meshShaderSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* shaderName )
    {
        if( !__ctx->_mesh.has( make_id( hmesh.h ) ) )
            return 0;

        bxGdiShaderFx_Instance* fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, shaderName, __ctx->_alloc_shaderFx );
        if( !fxI )
            return 0;

        const int index = _Mesh_index( hmesh );
        MeshFlags& flags = __ctx->_mesh._flags[index];
        if( flags.own_fxI )
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__ctx->_mesh._fxI[index], __ctx->_alloc_shaderFx );
        }
        flags.own_fxI = 1;
        __ctx->_mesh._fxI[index] = fxI;
        return fxI;
    }

    int meshShaderSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGdiShaderFx_Instance* fxI )
    {
        if ( !__ctx->_mesh.has( make_id( hmesh.h ) ) )
            return -1;

        const int index = _Mesh_index( hmesh );
        MeshFlags& flags = __ctx->_mesh._flags[index];
        if ( flags.own_fxI )
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &__ctx->_mesh._fxI[index], __ctx->_alloc_shaderFx );
        }
        flags.own_fxI = 0;
        __ctx->_mesh._fxI[index] = fxI;
        return 0;
    }

    bxGdiRenderSource* meshRenderSource( bxGfx_HMesh hmesh )
    {
        const id_t id = make_id( hmesh.h );
        if( !__ctx->_mesh.has( id ) )
            return nullptr;

        return __ctx->_mesh.rsource( id );
    }

    bxGdiShaderFx_Instance* meshShader( bxGfx_HMesh hmesh )
    {
        const id_t id = make_id( hmesh.h );
        if( !__ctx->_mesh.has( id ) )
            return nullptr;

        return __ctx->_mesh.fxI( id );
    }

    bxGfx_HInstanceBuffer instanceBuffeCreate( int nInstances )
    {
        bxGfx_HInstanceBuffer handle = { 0 };
        
        id_t id = __ctx->_instance.add();
        __ctx->_instance.allocateData( id, nInstances );

        handle.h = id.hash;
        return handle;
    }

    void instanceBufferRelease( bxGfx_HInstanceBuffer* hinstance )
    {
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( hinstance[0] ) );
        hinstance->h = 0;
    }
    
    int instanceBufferData( bxGfx_HInstanceBuffer hinstance, Matrix4* buffer, int bufferSize, int startIndex /*= 0 */ )
    {
        SYS_ASSERT( __ctx->_instance.has( make_id( hinstance.h ) ) );
        int result = 0;

        const InstanceContainer::Data data = __ctx->_instance.data( make_id( hinstance.h ) );
        const int endIndex = minOfPair( startIndex + bufferSize, data.count );
        const int count = endIndex - startIndex;
        
        memcpy( buffer, data.pose, count * sizeof( Matrix4 ) );
        
        return count;
    }

    int instanceBufferDataSet( bxGfx_HInstanceBuffer hinstance, const Matrix4* buffer, int bufferSize, int startIndex /*= 0 */ )
    {
        SYS_ASSERT( __ctx->_instance.has( make_id( hinstance.h ) ) );
        
        InstanceContainer::Data data = __ctx->_instance.data( make_id( hinstance.h ) );
        const int endIndex = minOfPair( startIndex + bufferSize, data.count );
        const int count = endIndex - startIndex;
        
        memcpy( data.pose + startIndex, buffer, count * sizeof( Matrix4 ) );

        return count;
    }

    bxGfx_World* worldCreate()
    {
        bxScopeBenaphore lock( __ctx->_lock_world );
        bxGfx_World* world = BX_NEW( __ctx->_alloc_world, bxGfx_World );
        world->flag_active = 1;
        int index = __ctx->_world.add( world );
        
        //bxGfx_HWorld result = { id.hash };
        return world;
    }

    void worldRelease( bxGfx_World** w )
    {
        if( !w[0] )
            return;
        //bxGfx::World* world = __ctx->world( h[0] );
        //if ( !world )
        //    return;

        w[0]->flag_active = 0;
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( w[0] ) );

        w[0] = 0;
    }

    bxGfx_HMeshInstance worldMeshAdd( bxGfx_World* world, bxGfx_HMesh hmesh, int nInstances )
    {
        //bxGfx::World* world = __ctx->world( hworld );
        //if( !world )
        //    return makeInvalidHandle<bxGfx_HMeshInstance>();
        
        bxGfx_HInstanceBuffer hinstance = instanceBuffeCreate( nInstances );
        bxGfx_HMeshInstance meshi = world->add( hmesh, hinstance );
        hashmap_t::cell_t* cell = hashmap::insert( __ctx->_map_meshInstanceToWorld, meshi.h );
        cell->value = size_t( world );

        //bxGfx_HMeshInstance meshi = makeMeshInstance( hmesh, hinstance );
        //bxGfx_World* foundHWorld = __ctx->lookupWorld( meshi );
        //if( !foundHWorld )
        //{
        //    SYS_ASSERT( world->meshFind( hmesh ) == -1 );
        //    SYS_ASSERT( world->instanceFind( hinstance ) == -1 );
        //    world->add( hmesh, hinstance );

        //    hashmap_t::cell_t* cell = hashmap::insert( __ctx->_map_meshInstanceToWorld, meshi.h );
        //    cell->value = size_t( world );
        //}
        //else
        //{
        //    bxLogWarning( "Mesh already in world" );
        //}

        return meshi;
    }

    void worldMeshRemove( bxGfx_HMeshInstance hmeshi )
    {
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( hmeshi ) );
    }
    void worldMeshRemoveAndRelease( bxGfx_HMeshInstance* hmeshi )
    {
        bxScopeBenaphore lock( __ctx->_lock_toRelease );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( hmeshi[0] ) );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( hMeshGet( hmeshi[0] ) ) );
        array::push_back( __ctx->_toRelease, bxGfx::ToReleaseEntry( hInstanceBufferGet( hmeshi[0] ) ) );

        hmeshi->h = 0;
    }

    void meshInstance( bxGfx_HMesh* hmesh, bxGfx_HInstanceBuffer* hinstance, bxGfx_HMeshInstance hmeshi )
    {
        hmesh[0] = hMeshGet( hmeshi );
        hinstance[0] = hInstanceBufferGet( hmeshi );
    }
    bxGfx_HMesh meshInstanceHMesh( bxGfx_HMeshInstance hmeshi )
    {
        return hMeshGet( hmeshi );
    }
    bxGfx_HInstanceBuffer meshInstanceHInstanceBuffer( bxGfx_HMeshInstance hmeshi )
    {
        return hInstanceBufferGet( hmeshi );
    }

    void worldBuildDrawListColorDepth( bxGfx_World* world, const bxGfxCamera& camera )
    {
        const bxGfx_World::Data& worldData = world->_data;

        bxGfx_HMesh* hmeshArray = worldData.mesh;
        bxGfx_HInstanceBuffer* hinstanceArray = worldData.instance;


        if( !world->_sList_color || world->_sList_color->capacity < worldData.size )
        {
            bxGdi::sortList_delete( &world->_sList_color );
            bxGdi::sortList_delete( &world->_sList_depth );
            bxGdi::sortList_new( &world->_sList_color, worldData.size, bxDefaultAllocator() );
            bxGdi::sortList_new( &world->_sList_depth, worldData.size, bxDefaultAllocator() );
        }

        bxGfx_SortListColor* colorList = world->_sList_color;
        bxGfx_SortListColor* depthList = world->_sList_depth;

        bxChunk colorChunk, depthChunk;
        bxChunk_create( &colorChunk, 1, colorList->capacity );
        bxChunk_create( &depthChunk, 1, depthList->capacity );

        const int n = worldData.size;
        for( int iitem = 0; iitem < n; ++iitem )
        {
            
        }
    }

    void worldDraw( bxGdiContext* ctx, bxGfx_World* world, const bxGfxCamera& camera )
    {
        if( !world->flag_active )
            return;

        MeshContainer* meshContainer = &__ctx->_mesh;
        InstanceContainer* instanceContainer = &__ctx->_instance;

        const bxGfx_World::Data& worldData = world->_data;

        bxGfx_HMesh* hmeshArray = worldData.mesh;
        bxGfx_HInstanceBuffer* hinstanceArray = worldData.instance;

        const int nHmesh = worldData.size;
        {
            float4_t* dataWorld = (float4_t*)bxGdi::buffer_map( ctx->backend(), __ctx->_buffer_instanceWorld, 0, __ctx->_maxInstances );
            float3_t* dataWorldIT = (float3_t*)bxGdi::buffer_map( ctx->backend(), __ctx->_buffer_instanceWorldIT, 0, __ctx->_maxInstances );
            
            array::clear( __ctx->_instanceOffset );
            u32 currentOffset = 0;
            u32 instanceCounter = 0;
            for( int imesh = 0; imesh < nHmesh; ++imesh )
            {
                InstanceContainer::Data idata = instanceContainer->data( make_id( hinstanceArray[imesh].h ) );
                
                for( int imatrix = 0; imatrix < idata.count; ++imatrix, ++instanceCounter )
                {
                    SYS_ASSERT( instanceCounter < __ctx->_maxInstances );

                    const u32 dataOffset = ( currentOffset + imatrix ) * 3;
                    const Matrix4 worldRows = transpose( idata.pose[imatrix] );
                    const Matrix3 worldITRows = inverse( idata.pose[imatrix].getUpper3x3() );

                    const float4_t* worldRowsPtr = (float4_t*)&worldRows;
                    memcpy( dataWorld + dataOffset, worldRowsPtr, sizeof( float4_t ) * 3 );

                    const float4_t* worldITRowsPtr = (float4_t*)&worldITRows;
                    memcpy( dataWorldIT + dataOffset    , worldITRowsPtr    , sizeof( float3_t ) );
                    memcpy( dataWorldIT + dataOffset + 1, worldITRowsPtr + 1, sizeof( float3_t ) );
                    memcpy( dataWorldIT + dataOffset + 2, worldITRowsPtr + 2, sizeof( float3_t ) );
                }

                array::push_back( __ctx->_instanceOffset, currentOffset );
                currentOffset += idata.count;
            }
            array::push_back( __ctx->_instanceOffset, currentOffset );

            ctx->backend()->unmap( __ctx->_buffer_instanceWorldIT.rs );
            ctx->backend()->unmap( __ctx->_buffer_instanceWorld.rs );
        }
        SYS_ASSERT( array::size( __ctx->_instanceOffset ) == nHmesh + 1 );
        bxGfx::FrameData fdata;
        bxGfx::frameData_fill( &fdata, camera, 1920, 1080 );
        ctx->backend()->updateCBuffer( __ctx->_cbuffer_frameData, &fdata );

        ctx->setBufferRO( __ctx->_buffer_instanceWorld, 0, bxGdi::eSTAGE_MASK_VERTEX );
        ctx->setBufferRO( __ctx->_buffer_instanceWorldIT, 1, bxGdi::eSTAGE_MASK_VERTEX );
        ctx->setCbuffer( __ctx->_cbuffer_frameData, 0, bxGdi::eSTAGE_MASK_VERTEX | bxGdi::eSTAGE_MASK_PIXEL );
        ctx->setCbuffer( __ctx->_cbuffer_instanceOffset, 1, bxGdi::eSTAGE_MASK_VERTEX );

        for( int imesh = 0; imesh < nHmesh; ++imesh )
        {
            bxGfx_HMesh hmesh = hmeshArray[imesh];
            const int meshDataIndex = _Mesh_index( hmesh );
            
            bxGdiRenderSource* rsource = meshContainer->_rsource[meshDataIndex];
            bxGdiShaderFx_Instance* fxI = meshContainer->_fxI[meshDataIndex];

            u32 instanceOffset = __ctx->_instanceOffset[imesh];
            u32 instanceCount = __ctx->_instanceOffset[imesh + 1] - instanceOffset;
            ctx->backend()->updateCBuffer( __ctx->_cbuffer_instanceOffset, &instanceOffset );

            bxGdi::renderSource_enable( ctx, rsource );
            bxGdi::shaderFx_enable( ctx, fxI, 0 );

            bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
            bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, instanceCount );
        }
    }
}///
