#include "renderer.h"
#include <util/debug.h>
#include <util/common.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/id_array.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>
#include <util/random.h>
#include <util/time.h>
#include <util/handle_manager.h>
#include <util/float16.h>

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_sort_list.h>

#include <gfx/gfx_type.h>

#include <algorithm>

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

    //bxGfx_HMeshInstance add( bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance )
    //{
    //    int index = meshFind( hmesh );
    //    if( index != -1 )
    //        return index;

    //    if( _data.size + 1 > _data.capacity )
    //    {
    //        const int newcap = _data.capacity * 2 + 8;
    //        allocateData( &_data, newcap, _alloc_data );
    //    }

    //    

    //    bxGfx_World::Data& data = _data;
    //    index = data.size++;

    //    data.mesh[index] = hmesh;
    //    data.instance[index] = hinstance;

    //    return index;
    //}

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
        
        bxGfx_HMeshInstance meshi = { 0 };
        //bxGfx_HInstanceBuffer hinstance = instanceBuffeCreate( nInstances );
        //bxGfx_HMeshInstance meshi = world->add( hmesh, hinstance );
        //hashmap_t::cell_t* cell = hashmap::insert( __ctx->_map_meshInstanceToWorld, meshi.h );
        //cell->value = size_t( world );

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
        bxGfx_SortListDepth* depthList = world->_sList_depth;

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


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#include "gfx_private.h"
namespace bx
{
    float gfxCameraAspect( const GfxCamera* cam )
    {
        return (abs( cam->vAperture ) < 0.0001f) ? cam->hAperture : cam->hAperture / cam->vAperture;
    }
    float gfxCameraFov( const GfxCamera* cam )
    {
        return 2.f * atan( ( 0.5f * cam->hAperture ) / ( cam->focalLength * 0.03937f ) );
    }
    Vector3 gfxCameraEye( const GfxCamera* cam )
    {
        return cam->world.getTranslation();
    }
    Vector3 gfxCameraDir( const GfxCamera* cam )
    {
        return -cam->world.getCol2().getXYZ();
    }

    void gfxCameraViewport( GfxViewport* vp, const GfxCamera* cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
    {
        const int windowWidth = dstWidth;
        const int windowHeight = dstHeight;

        const float aspectRT = (float)srcWidth / (float)srcHeight;
        const float aspectCamera = gfxCameraAspect( cam );

        int imageWidth;
        int imageHeight;
        int offsetX = 0, offsetY = 0;


        if( aspectCamera > aspectRT )
        {
            imageWidth = windowWidth;
            imageHeight = (int)( windowWidth / aspectCamera + 0.0001f );
            offsetY = windowHeight - imageHeight;
            offsetY = offsetY / 2;
        }
        else
        {
            float aspect_window = (float)windowWidth / (float)windowHeight;
            if( aspect_window <= aspectRT )
            {
                imageWidth = windowWidth;
                imageHeight = (int)( windowWidth / aspectRT + 0.0001f );
                offsetY = windowHeight - imageHeight;
                offsetY = offsetY / 2;
            }
            else
            {
                imageWidth = (int)( windowHeight * aspectRT + 0.0001f );
                imageHeight = windowHeight;
                offsetX = windowWidth - imageWidth;
                offsetX = offsetX / 2;
            }
        }
        vp[0] = GfxViewport( offsetX, offsetY, imageWidth, imageHeight );
    }

    void gfxCameraComputeMatrices( GfxCamera* cam )
    {
        const float fov = gfxCameraFov( cam );
        const float aspect = gfxCameraAspect( cam );

        cam->view = inverse( cam->world );
        cam->proj = Matrix4::perspective( fov, aspect, cam->zNear, cam->zFar );
        cam->viewProj = cam->proj * cam->view;
    }


    void gfxCameraWorldMatrixSet( GfxCamera* cam, const Matrix4& world )
    {
        cam->world = world;
    }
    Matrix4 gfxCameraWorldMatrixGet( const GfxCamera* camera )
    {
        return camera->world;
    }

    void gfxViewFrameParamsFill( GfxViewFrameParams* fparams, const GfxCamera* camera, int rtWidth, int rtHeight )
    {
        //SYS_STATIC_ASSERT( sizeof( FrameData ) == 376 );

        const Matrix4 sc = Matrix4::scale( Vector3( 1, 1, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 0, 0, 1 ) );
        const Matrix4 proj = sc * tr * camera->proj;

        fparams->_camera_view = camera->view;
        fparams->_camera_proj = proj;
        fparams->_camera_viewProj = proj * camera->view;
        fparams->_camera_world = camera->world;

        const float fov = gfxCameraFov( camera );
        const float aspect = gfxCameraAspect( camera );

        fparams->_camera_fov = fov;
        fparams->_camera_aspect = aspect;

        const float zNear = camera->zNear;
        const float zFar = camera->zFar;
        fparams->_camera_zNear = zNear;
        fparams->_camera_zFar = zFar;
        fparams->_reprojectDepthScale = ( zFar - zNear ) / ( -zFar * zNear );
        fparams->_reprojectDepthBias = zFar / ( zFar * zNear );

        fparams->_renderTarget_rcp = float2_t( 1.f / (float)rtWidth, 1.f / (float)rtHeight );
        fparams->_renderTarget_size = float2_t( (float)rtWidth, (float)rtHeight );

        //frameData->cameraParams = Vector4( fov, aspect, camera.params.zNear, camera.params.zFar );
        {
            const float m11 = proj.getElem( 0, 0 ).getAsFloat();//getCol0().getX().getAsFloat();
            const float m22 = proj.getElem( 1, 1 ).getAsFloat();//getCol1().getY().getAsFloat();
            const float m33 = proj.getElem( 2, 2 ).getAsFloat();//getCol2().getZ().getAsFloat();
            const float m44 = proj.getElem( 3, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            const float m13 = proj.getElem( 0, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();
            const float m23 = proj.getElem( 1, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            fparams->_reprojectInfo = float4_t( 1.f / m11, 1.f / m22, m33, -m44 );
            //frameData->_reprojectInfo = float4_t( 
            //    -2.f / ( (float)rtWidth*m11 ), 
            //    -2.f / ( (float)rtHeight*m22 ), 
            //    (1.f - m13) / m11, 
            //    (1.f + m23) / m22 );
            fparams->_reprojectInfoFromInt = float4_t(
                ( -fparams->_reprojectInfo.x * 2.f ) * fparams->_renderTarget_rcp.x,
                ( -fparams->_reprojectInfo.y * 2.f ) * fparams->_renderTarget_rcp.y,
                fparams->_reprojectInfo.x,
                fparams->_reprojectInfo.y
                );
            //frameData->_reprojectInfoFromInt = float4_t(
            //    frameData->_reprojectInfo.x,
            //    frameData->_reprojectInfo.y,
            //    frameData->_reprojectInfo.z + frameData->_reprojectInfo.x * 0.5f,
            //    frameData->_reprojectInfo.w + frameData->_reprojectInfo.y * 0.5f
            //    );
        }

        m128_to_xyzw( fparams->_camera_eyePos.xyzw, Vector4( gfxCameraEye( camera ), oneVec ).get128() );
        m128_to_xyzw( fparams->_camera_viewDir.xyzw, Vector4( gfxCameraDir( camera ), zeroVec ).get128() );

        //m128_to_xyzw( frameData->_renderTarget_rcp_size.xyzw, Vector4( 1.f / float( rtWidth ), 1.f / float( rtHeight ), float( rtWidth ), float( rtHeight ) ).get128() );
    }

    ////
    //
    void gfxContextStartup( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxContext* g = BX_NEW( bxDefaultAllocator(), GfxContext );

        gfxViewCreate( &g->_cmdQueue._view, dev, 1024 );
        
        const int fbWidth = 1920;
        const int fbHeight = 1080;
        g->_framebuffer[eFB_COLOR0] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_DEPTH]  = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        {
            g->_fxISky = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "sky" );
        }

        {

            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( GfxMeshInstance ), 64, bxDefaultAllocator(), 16 );
            g->_allocMesh = dpoolAlloc;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( GfxCamera ), 16, bxDefaultAllocator(), 16 );
            g->_allocCamera = dpoolAlloc;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( Matrix4 ), 64, bxDefaultAllocator(), 16 );
            g->_allocIDataSingle = dpoolAlloc;
        }
        {
            g->_allocScene = bxDefaultAllocator();
            g->_allocIDataMulti = bxDefaultAllocator();
        }

        gfxGlobalResourcesStartup( &g->_globalResources, dev, resourceManager );
        gfxMaterialManagerStartup( &g->_materialManager, dev, resourceManager );

        gfx[0] = g;
    }

    void gfxContextShutdown( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxContext* g = gfx[0];

        gfxMaterialManagerShutdown( &g->_materialManager, dev, resourceManager );
        gfxGlobalResourcesShutdown( &g->_globalResources, dev, resourceManager );

        {
            g->_allocIDataMulti = nullptr;
            g->_allocScene = nullptr;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocIDataSingle;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocIDataSingle );
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocCamera;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocCamera );
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocMesh;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocMesh );
        }
        
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &g->_fxISky );
        }

        for ( int i = 0; i < eFB_COUNT; ++i )
            dev->releaseTexture( &g->_framebuffer[i] );

        gfxViewDestroy( &g->_cmdQueue._view, dev );

        BX_DELETE0( bxDefaultAllocator(), gfx[0] );
    }

    void gfxContextTick( GfxContext* gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        gfx->_lockActorsToRelease.lock();

        for( int i = 0; i < array::size( gfx->_actorsToRelease ); ++i )
        {
            GfxActor* actor = gfx->_actorsToRelease[i];

            if( actor->isScene() )
            {
                GfxScene* scene = actor->isScene();
                for( int j = 0; j < scene->_data.size; ++j )
                {
                    u32 meshIhandle = scene->_data.meshHandle[j];
                    GfxActor* meshActor = nullptr;
                    if( gfx->_handles.get( ActorHandleManager::Handle( meshIhandle ), &meshActor ) != -1 )
                    {
                        gfxContextHandleRemove( gfx, meshIhandle );
                        array::push_back( gfx->_actorsToRelease, meshActor );
                    }
                }

                BX_DELETE0( gfx->_allocScene, scene );
            }
            else if( actor->isMeshInstance() )
            {
                GfxMeshInstance* meshI = actor->isMeshInstance();
                
                bxAllocator* allocIdata = (meshI->_idata.count == 1) ? gfx->_allocIDataSingle : gfx->_allocIDataMulti;
                allocIdata->free( meshI->_idata.pose );

                BX_DELETE0( gfx->_allocMesh, meshI );
            }
            else if( actor->isCamera() )
            {
                GfxCamera* camera = actor->isCamera();
                BX_DELETE0( gfx->_allocCamera, camera );
            }
            else
            {
                SYS_ASSERT( false );
            }

        }
        array::clear( gfx->_actorsToRelease );
        gfx->_lockActorsToRelease.unlock();
    }

    void gfxContextFrameBegin( GfxContext* gfx, bxGdiContext* gdi )
    {
        (void)gfx;
        gdi->clear();
    }

    void gfxContextFrameEnd( GfxContext* gfx, bxGdiContext* gdi )
    {
        (void)gfx;
        gdi->backend()->swap();
    }

    void gfxCommandQueueAcquire( GfxCommandQueue** cmdq, GfxContext* ctx, bxGdiContext* gdiContext )
    {
        GfxCommandQueue* cmdQueue = &ctx->_cmdQueue;
        SYS_ASSERT( cmdQueue->_acquireCounter == 0 );
        ++cmdQueue->_acquireCounter;
        cmdQueue->_ctx = ctx;
        cmdQueue->_gdiContext = gdiContext;
        cmdq[0] = cmdQueue;
    }
    void gfxCommandQueueRelease( GfxCommandQueue** cmdq )
    {
        cmdq[0]->_gdiContext = nullptr;
        cmdq[0]->_ctx = nullptr;
        SYS_ASSERT( cmdq[0]->_acquireCounter == 1 );
        --cmdq[0]->_acquireCounter;
        cmdq[0] = nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    ///
    void gfxSubmitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName )
    {
        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        bxGdi::renderSource_enable( ctx, gr->mesh.fullScreenQuad );
        bxGdi::shaderFx_enable( ctx, fxI, passName );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( gr->mesh.fullScreenQuad->vertexBuffers->numElements, 0 );
    }

    void gfxCopyTextureRGBA( bxGdiContext* ctx, bxGdiTexture outputTexture, bxGdiTexture inputTexture )
    {
        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        ctx->changeRenderTargets( &outputTexture, 1 );
        bxGdi::context_setViewport( ctx, outputTexture );
        gfxSubmitFullScreenQuad( ctx, gr->fx.texUtils, "copy_rgba" );
    }

    void gfxRasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, float cameraAspect )
    {
        ctx->changeToMainFramebuffer();

        bxGdiTexture colorTexture = colorFB; // _framebuffer[bxGfx::eFRAMEBUFFER_COLOR];
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();
        GfxViewport viewport = bx::gfx::computeViewport( cameraAspect, backBuffer.width, backBuffer.height, colorTexture.width, colorTexture.height );

        ctx->setViewport( viewport );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );

        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        bxGdiShaderFx_Instance* fxI = gr->fx.texUtils;
        fxI->setTexture( "gtexture", colorTexture );
        fxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR ) );

        gfxSubmitFullScreenQuad( ctx, fxI, "copy_rgba" );
    }

    GfxGlobalResources* gfxGlobalResourcesGet()
    {
        return GfxContext::_globalResources;
    }
    bxGdiShaderFx_Instance* gfxMaterialFind( const char* name )
    {
        const u64 key = gfxMaterialManagerCreateNameHash( name );
        hashmap_t::cell_t* cell = hashmap::lookup( GfxContext::_materialManager->_map, key );

        return (cell) ? (bxGdiShaderFx_Instance*)cell->value : 0;
    }

    //////////////////////////////////////////////////////////////////////////
    ///
    void gfxCameraCreate( GfxCamera** camera, GfxContext* ctx )
    {
        GfxCamera* c = BX_NEW( ctx->_allocCamera, GfxCamera );
        c->_ctx = ctx;
        c->_internalHandle = gfxContextHandleAdd( ctx, c );

        camera[0] = c;
    }

    void gfxCameraDestroy( GfxCamera** camera )
    {
        GfxCamera* c = camera[0];

        if( !c )
            return;

        if( !gfxContextHandleValid( c->_ctx, c->_internalHandle ) )
            return;

        gfxContextHandleRemove( c->_ctx, c->_internalHandle );
        gfxContextActorRelease( c->_ctx, c );

        camera[0] = nullptr;
    }

    void gfxMeshInstanceCreate( GfxMeshInstance** meshI, GfxContext* ctx, int numInstances )
    {
        GfxMeshInstance* m = BX_NEW( ctx->_allocMesh, GfxMeshInstance );
        m->_ctx = ctx;
        m->_internalHandle = gfxContextHandleAdd( ctx, m );
        
        bxAllocator* allocInstance = ( numInstances == 1 ) ? ctx->_allocIDataSingle : ctx->_allocIDataMulti;
        m->_idata.pose = (Matrix4*)allocInstance->alloc( numInstances * sizeof( Matrix4 ), 16 );
        m->_idata.count = numInstances;

        meshI[0] = m;
    }

    void gfxMeshInstanceDestroy( GfxMeshInstance** meshI )
    {
        GfxMeshInstance* m = meshI[0];
        if( !m )
            return;

        if( !gfxContextHandleValid( m->_ctx, m->_internalHandle ) )
            return;

        if( m->_scene )
        {
            gfxSceneMeshInstanceRemove( m->_scene, m );
        }
        gfxContextHandleRemove( m->_ctx, m->_internalHandle );
        gfxContextActorRelease( m->_ctx, m );

        meshI[0] = nullptr;
    }
    bxGdiRenderSource* gfxMeshInstanceRenderSourceGet( GfxMeshInstance* meshI )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        return meshI->_rsource;
    }
    bxGdiShaderFx_Instance* gfxMeshInstanceFxGet( GfxMeshInstance* meshI )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        return meshI->_fxI;
    }
    void gfxMeshInstanceDataSet( GfxMeshInstance* meshI, const GfxMeshInstanceData& data )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );

        if( data.mask & GfxMeshInstanceData::eMASK_RENDER_SOURCE )
        {
            meshI->_rsource = data.rsource;
        }
        if( data.mask & GfxMeshInstanceData::eMASK_SHADER_FX )
        {
            meshI->_fxI = data.fxInstance;
        }
        if( data.mask & GfxMeshInstanceData::eMASK_LOCAL_AABB )
        {
            memcpy( &meshI->_localAABB[0], &data.localAABB[0], 6 * sizeof( f32 ) );
        }

        if( data.mask && meshI->_scene )
        {
            gfxSceneDataRefresh( meshI->_scene, meshI->_internalHandle );
        }
    }

    void gfxMeshInstanceWorldMatrixSet( GfxMeshInstance* meshI, const Matrix4* matrices, int nMatrices )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        SYS_ASSERT( nMatrices > 0 );

        int numToSet = minOfPair( nMatrices, meshI->_idata.count );
        memcpy( meshI->_idata.pose, matrices, numToSet * sizeof( Matrix4 ) );
    }


    void gfxSceneCreate( GfxScene** scene, GfxContext* ctx )
    {
        GfxScene* s = BX_NEW( ctx->_allocScene, GfxScene );
        s->_ctx = ctx;
        s->_internalHandle = gfxContextHandleAdd( ctx, s );

        scene[0] = s;
    }
    void gfxSceneDestroy( GfxScene** scene )
    {
        GfxScene* s = scene[0];
        if( !s )
            return;

        if( !gfxContextHandleValid( s->_ctx, s->_internalHandle ) )
            return;

        gfxContextHandleRemove( s->_ctx, s->_internalHandle );
        gfxContextActorRelease( s->_ctx, s );
    }

    void gfxSceneMeshInstanceAdd( GfxScene* scene, GfxMeshInstance* meshI )
    {
        if ( !gfxContextHandleValid( scene->_ctx, scene->_internalHandle ) )
            return;
        
        if ( !gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) )
            return;

        GfxScene::Cmd cmd;
        cmd.handle = meshI->_internalHandle;
        cmd.op = GfxScene::Cmd::eOP_ADD;

        scene->_lockCmd.lock();
        array::push_back( scene->_cmd, cmd );
        scene->_lockCmd.unlock();
    }
    void gfxSceneMeshInstanceRemove( GfxScene* scene, GfxMeshInstance* meshI )
    {
        if ( !gfxContextHandleValid( scene->_ctx, scene->_internalHandle ) )
            return;

        if ( !gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) )
            return;

        GfxScene::Cmd cmd;
        cmd.handle = meshI->_internalHandle;
        cmd.op = GfxScene::Cmd::eOP_REMOVE;

        scene->_lockCmd.lock();
        array::push_back( scene->_cmd, cmd );
        scene->_lockCmd.unlock();

        meshI->_scene = nullptr;
    }
    
    namespace 
    {
        inline GfxMeshInstance* _MeshInstanceFromHandle( GfxContext* ctx, u32 handle )
        {
            GfxActor* actor = nullptr;
            if ( gfxContextHandleActorGet_noLock( &actor, ctx, handle ) != -1 )
            {
                return actor->isMeshInstance();
            }
            return nullptr;
        }

        void sceneBuildSortListColorDepth( bxChunk* colorChunk, bxChunk* depthChunk, GfxScene* scene, const GfxCamera* camera )
        {
            const GfxScene::Data& data = scene->_data;

            if ( !scene->_sListColor || scene->_sListColor->capacity < data.size )
            {
                bxGdi::sortList_delete( &scene->_sListColor );
                bxGdi::sortList_delete( &scene->_sListDepth );
                bxGdi::sortList_new( &scene->_sListColor, data.size * 2, bxDefaultAllocator() );
                bxGdi::sortList_new( &scene->_sListDepth, data.size * 2, bxDefaultAllocator() );
            }

            GfxSortListColor* colorList = scene->_sListColor;
            GfxSortListDepth* depthList = scene->_sListDepth;

            //bxChunk colorChunk, depthChunk;
            bxChunk_create( colorChunk, 1, data.size );
            bxChunk_create( depthChunk, 1, data.size );

            GfxViewFrustum frustum = bx::gfx::viewFrustumExtract( camera->viewProj );

            const int n = data.size;
            for ( int iitem = colorChunk->begin; iitem < colorChunk->end; ++iitem )
            {
                const GfxInstanceData& idata = data.idata[iitem];
                const bxAABB& localAABB = data.bbox[iitem];

                for ( int ii = 0; ii < idata.count; ++ii )
                {
                    const Matrix4& world = idata.pose[ii];
                    bxAABB worldAABB = bxAABB::transform( world, localAABB );

                    bool inFrustum = bx::gfx::viewFrustumAABBIntersect( frustum, worldAABB.min, worldAABB.max ).getAsBool();
                    if ( !inFrustum )
                        continue;

                    u32 hashMesh = data.rsource[iitem]->sortHash;
                    u16 hashMesh16 = u16( hashMesh >> 16 ) ^ u16( hashMesh & 0xFFFF );
                    u32 hashShader = data.fxInstance[iitem]->sortHash( 0 );

                    float depth = bx::gfx::cameraDepth( camera->world, world.getTranslation() ).getAsFloat();
                    u16 depth16 = float_to_half_fast3( fromF32( depth ) ).u;

                    GfxSortItemColor colorSortItem;
                    GfxSortKeyColor& colorSortKey = colorSortItem.key;
                    colorSortKey.instance = ii;
                    colorSortKey.mesh = hashMesh16;
                    colorSortKey.shader = hashShader;
                    colorSortKey.layer = 8;

                    GfxSortItemDepth depthSortItem;
                    GfxSortKeyDepth& depthSortKey = depthSortItem.key;
                    depthSortKey.depth = depth16;

                    colorSortItem.index = iitem;
                    depthSortItem.index = iitem;

                    bxGdi::sortList_chunkAdd( colorList, colorChunk, colorSortItem );
                    bxGdi::sortList_chunkAdd( depthList, depthChunk, depthSortItem );
                }
            }

            bxGdi::sortList_sortLess( colorList, *colorChunk );
            bxGdi::sortList_sortLess( depthList, *depthChunk );
        }

        template< class Tlist >
        void viewUploadInstanceData( bxGdiContext* gdi, GfxView& view, const GfxScene* scene, const Tlist* sList, int begin, int end )
        {
            float4_t* dataWorld = (float4_t*)bxGdi::buffer_map( gdi->backend(), view._instanceWorldBuffer, 0, view._maxInstances );
            float3_t* dataWorldIT = (float3_t*)bxGdi::buffer_map( gdi->backend(), view._instanceWorldITBuffer, 0, view._maxInstances );

            const GfxScene::Data& data = scene->_data;

            array::clear( view._instanceOffsetArray );
            u32 currentOffset = 0;
            u32 instanceCounter = 0;
            for ( int i = begin; i < end; ++i )
            {
                const Tlist::ItemType& item = sList->items[i];
                const GfxInstanceData idata = data.idata[item.index];

                for ( int imatrix = 0; imatrix < idata.count; ++imatrix, ++instanceCounter )
                {
                    SYS_ASSERT( instanceCounter < (u32)view._maxInstances );

                    const u32 dataOffset = (currentOffset + imatrix) * 3;
                    const Matrix4 worldRows = transpose( idata.pose[imatrix] );
                    const Matrix3 worldITRows = inverse( idata.pose[imatrix].getUpper3x3() );

                    const float4_t* worldRowsPtr = (float4_t*)&worldRows;
                    memcpy( dataWorld + dataOffset, worldRowsPtr, sizeof( float4_t ) * 3 );

                    const float4_t* worldITRowsPtr = (float4_t*)&worldITRows;
                    memcpy( dataWorldIT + dataOffset, worldITRowsPtr, sizeof( float3_t ) );
                    memcpy( dataWorldIT + dataOffset + 1, worldITRowsPtr + 1, sizeof( float3_t ) );
                    memcpy( dataWorldIT + dataOffset + 2, worldITRowsPtr + 2, sizeof( float3_t ) );
                }

                array::push_back( view._instanceOffsetArray, currentOffset );
                currentOffset += idata.count;
            }
            array::push_back( view._instanceOffsetArray, currentOffset );

            gdi->backend()->unmap( view._instanceWorldITBuffer.rs );
            gdi->backend()->unmap( view._instanceWorldBuffer.rs );

            SYS_ASSERT( array::size( view._instanceOffsetArray ) == end + 1 );

        }

        void sortListColorSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene* scene, const GfxSortListColor* sList, int begin, int end )
        {
            const GfxScene::Data& data = scene->_data;
            for ( int i = begin; i < end; ++i )
            {
                const GfxSortItemColor& item = sList->items[i];
                int meshDataIndex = item.index;

                bxGdiRenderSource* rsource = data.rsource[meshDataIndex];
                bxGdiShaderFx_Instance* fxI = data.fxInstance[meshDataIndex];

                u32 instanceOffset = view._instanceOffsetArray[i];
                u32 instanceCount = view._instanceOffsetArray[i + 1] - instanceOffset;
                gdi->backend()->updateCBuffer( view._instanceOffsetBuffer, &instanceOffset );

                bxGdi::renderSource_enable( gdi, rsource );
                bxGdi::shaderFx_enable( gdi, fxI, 0 );

                bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
                bxGdi::renderSurface_drawIndexedInstanced( gdi, surf, instanceCount );
            }
        }
        void sortListDepthSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene* scene, const GfxSortListDepth* sList, int begin, int end )
        {
            const GfxScene::Data& data = scene->_data;
            for( int i = begin; i < end; ++i )
            {
                const GfxSortListDepth::ItemType& item = sList->items[i];
                int meshDataIndex = item.index;

                bxGdiRenderSource* rsource = data.rsource[meshDataIndex];

                u32 instanceOffset = view._instanceOffsetArray[i];
                u32 instanceCount = view._instanceOffsetArray[i + 1] - instanceOffset;
                gdi->backend()->updateCBuffer( view._instanceOffsetBuffer, &instanceOffset );

                bxGdi::renderSource_enable( gdi, rsource );

                bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
                bxGdi::renderSurface_drawIndexedInstanced( gdi, surf, instanceCount );
            }
        }
    }///



    void gfxSceneDraw( GfxScene* scene, GfxCommandQueue* cmdq, const GfxCamera* camera )
    {
        GfxContext* ctx = scene->_ctx;
        bxScopeRecursiveBenaphore handlesLock( ctx->_lockHandles );
        {
            bxScopeRecursiveBenaphore lock( scene->_lockCmd );
            int nCmd = array::size( scene->_cmd );
            for( int i = 0; i < nCmd; ++i )
            {
                const GfxScene::Cmd cmd = scene->_cmd[i];

                switch( cmd.op )
                {
                case GfxScene::Cmd::eOP_ADD:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if( meshInstance )
                        {
                            gfxSceneDataAdd( &scene->_data, meshInstance );
                        }
                    }break;
                case GfxScene::Cmd::eOP_REMOVE:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if ( meshInstance )
                        {
                            int index = gfxSceneDataFind( scene->_data, cmd.handle );
                            gfxSceneDataRemove( &scene->_data, index );

                            
                        }
                    }break;
                case GfxScene::Cmd::eOP_REFRESH:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if ( meshInstance )
                        {
                            int index = gfxSceneDataFind( scene->_data, cmd.handle );
                            gfxSceneDataRefresh( &scene->_data, ctx, index );
                        }
                    }break;
                }//
            }
            array::clear( scene->_cmd );
        }

        bxGdiContext* gdi = cmdq->_gdiContext;
        

        const GfxScene::Data& data = scene->_data;
        int nMeshes = scene->_data.size;
        if( !nMeshes )
            return;

        bxChunk colorChunk, depthChunk;
        sceneBuildSortListColorDepth( &colorChunk, &depthChunk, scene, camera );

        GfxSortListColor* colorList = scene->_sListColor;
        GfxSortListDepth* depthList = scene->_sListDepth;

        GfxView& view = cmdq->_view;
        
        GfxViewFrameParams viewParams;
        gfxViewFrameParamsFill( &viewParams, camera, ctx->_framebuffer->width, ctx->_framebuffer->height );
        gdi->backend()->updateCBuffer( view._viewParamsBuffer, &viewParams );

        gdi->setBufferRO( view._instanceWorldBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setBufferRO( view._instanceWorldITBuffer, 1, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setCbuffer( view._viewParamsBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX | bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setCbuffer( view._instanceOffsetBuffer, 1, bxGdi::eSTAGE_MASK_VERTEX );

        /// depth prepass
        {
            viewUploadInstanceData( gdi, view, scene, depthList, depthChunk.begin, depthChunk.current );
            gdi->changeRenderTargets( nullptr, 0, ctx->_framebuffer[eFB_DEPTH] );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );

            bxGdiShaderFx_Instance* fxI = gfxGlobalResourcesGet()->fx.utils;
            bxGdi::shaderFx_enable( gdi, fxI, "zPrepassDepthOnly" );
            sortListDepthSubmit( gdi, view, scene, depthList, depthChunk.begin, depthChunk.current );
        }

        /// sky
        {
            gdi->changeRenderTargets( &ctx->_framebuffer[eFB_COLOR0], 1 );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );

            bxGdiShaderFx_Instance* fxI = ctx->_fxISky;
            gfxSubmitFullScreenQuad( gdi, fxI, "skyPreetham" );
            
        }

        /// color pass
        {
            viewUploadInstanceData( gdi, view, scene, colorList, colorChunk.begin, colorChunk.current );

            gdi->changeRenderTargets( &ctx->_framebuffer[eFB_COLOR0], 1, ctx->_framebuffer[ eFB_DEPTH ] );
            //gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );

            sortListColorSubmit( gdi, view, scene, colorList, colorChunk.begin, colorChunk.current );
        }



        gfxRasterizeFramebuffer( gdi, ctx->_framebuffer[eFB_COLOR0], gfxCameraAspect( camera ) );
    }

    



}///












