#include "engine.h"
#include <system/window.h>
#include <system/input.h>

#include <util/config.h>
#include <util/string_util.h>
#include <util/array.h>
#include <util/queue.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include <util/array_util.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>

#include <gfx/gfx_gui.h>
#include <gfx/gfx_debug_draw.h>

#include <atomic>
#include <mutex>
#include <algorithm>

namespace bx
{
void Engine::startup( Engine* e )
{
    bxConfig::global_init();

    bxWindow* win = bxWindow_get();
    const char* assetDir = bxConfig::global_string( "assetDir" );
    e->resource_manager = bxResourceManager::startup( assetDir );
    bxGdi::backendStartup( &e->gdi_device, (uptr)win->hwnd, win->width, win->height, win->full_screen );

    cameraManagerStartup( &e->camera_manager, e->gfx_context );

    e->gdi_context = BX_NEW( bxDefaultAllocator(), bxGdiContext );
    e->gdi_context->_Startup( e->gdi_device );

    bxGfxGUI::_Startup( e->gdi_device, win );
    bxGfxDebugDraw::_Startup( e->gdi_device );

    gfxContextStartup( &e->gfx_context, e->gdi_device );
    phxContextStartup( &e->phx_context, 4 );

    e->_camera_script_callback = BX_NEW( bxDefaultAllocator(), CameraManagerSceneScriptCallback );
    e->_camera_script_callback->_gfx = e->gfx_context;
    e->_camera_script_callback->_menago = e->camera_manager;
    e->_camera_script_callback->_current = nullptr;
 
    e->_graph_script_callback = BX_NEW( bxDefaultAllocator(), GraphSceneScriptCallback );


    graphContextStartup();

    rmt_CreateGlobalInstance( &e->_remotery );
}
void Engine::shutdown( Engine* e )
{
    rmt_DestroyGlobalInstance( e->_remotery );
    e->_remotery = 0;

    graphContextShutdown();

    BX_DELETE0( bxDefaultAllocator(), e->_graph_script_callback );
    BX_DELETE0( bxDefaultAllocator(), e->_camera_script_callback );

    phxContextShutdown( &e->phx_context );
    gfxContextShutdown( &e->gfx_context, e->gdi_device );

    cameraManagerShutdown( &e->camera_manager );

    bxGfxDebugDraw::_Shutdown( e->gdi_device );
    bxGfxGUI::_Shutdown( e->gdi_device, bxWindow_get() );

    e->gdi_context->_Shutdown();
    BX_DELETE0( bxDefaultAllocator(), e->gdi_context );

    bxGdi::backendShutdown( &e->gdi_device );
    bxResourceManager::shutdown( &e->resource_manager );
    
    bxConfig::global_deinit();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void Scene::startup( Scene* scene, Engine* engine )
{
    gfxSceneCreate( &scene->gfx, engine->gfx_context );
    phxSceneCreate( &scene->phx, engine->phx_context );
}

void Scene::shutdown( Scene* scene, Engine* engine )
{
    bx::phxSceneDestroy( &scene->phx );
    bx::gfxSceneDestroy( &scene->gfx );
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void DevCamera::startup( DevCamera* devCamera, Scene* scene, Engine* engine )
{
    gfxCameraCreate( &devCamera->camera, engine->gfx_context );
    engine->camera_manager->add( devCamera->camera, "dev_camera" );
    engine->camera_manager->stack()->push( devCamera->camera );
}

void DevCamera::shutdown( DevCamera* devCamera, Engine* engine )
{
    engine->camera_manager->remove( devCamera->camera );
    gfxCameraDestroy( &devCamera->camera );
}

void DevCamera::tick( const bxInput* input, float deltaTime )
{
    const bxInput_Mouse* inputMouse = &input->mouse;
    input_ctx.updateInput( inputMouse->currentState()->lbutton
                         , inputMouse->currentState()->mbutton
                         , inputMouse->currentState()->rbutton
                         , inputMouse->currentState()->dx
                         , inputMouse->currentState()->dy
                         , 0.01f
                         , deltaTime );

    const Matrix4 newCameraMatrix = input_ctx.computeMovement( gfxCameraWorldMatrixGet( camera ), 0.15f );
    gfxCameraWorldMatrixSet( camera, newCameraMatrix );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace AttributeType
{
    enum Enum : u8
    {
        eINT = 0,
        eFLOAT,
        eFLOAT3,
        eSTRING,

        eTYPE_COUNT,
    };
    static const unsigned char _stride[eTYPE_COUNT] =
    {
        sizeof( i32 ),
        sizeof( f32 ),
        sizeof( f32 ) * 3,
        sizeof( void* ),
    };
    static const unsigned char _alignment[eTYPE_COUNT] =
    {
        4,
        4,
        4,
        8,
    };

    struct String
    {
        char* c_str;
    };
}///

struct AttributeStruct
{
    union Name
    {
        u64 hash = 0;
        char str[8];
    };

    void* _memory_handle = nullptr;
    i32 _size = 0;
    i32 _capacity = 0;

    AttributeType::Enum* _type = nullptr;
    Name* _name = nullptr;
    u16* _offset = nullptr;
    
    array_t< u8 > _default_values;

    int find( const char* name ) const
    {
        Name name8;
        int i = 0;
        while( name[i] && i < 8 )
        {
            name8.str[i] = name[i];
            ++i;
        }
        //const Name* name8 = (Name*)name;
        for( int i = 0; i < _size; ++i )
        {
            if( name8.hash == _name[i].hash )
                return i;
        }
        return -1;
    }

    void alloc( int newCapacity )
    {
        int memSize = newCapacity * ( sizeof( Name ) + sizeof( AttributeType::Enum ) + sizeof( *_offset ) );
        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );
        AttributeType::Enum* newType = chunker.add< AttributeType::Enum >( newCapacity );
        Name* newName = chunker.add< Name >( newCapacity );
        u16* newOffset = chunker.add< u16 >( newCapacity );

        chunker.check();

        if( _size )
        {
            memcpy( newType, _type, _size * sizeof( *_type ) );
            memcpy( newName, _name, _size * sizeof( *_name ) );
            memcpy( newOffset, _offset, _size * sizeof( *_offset ) );
        }
        BX_FREE( bxDefaultAllocator(), _memory_handle );

        _memory_handle = mem;
        _type = newType;
        _name = newName;
        _offset = newOffset;
        _capacity = newCapacity;
    }
    void free()
    {
        for( int i = 0; i < _size; ++i )
        {
            if( _type[i] == AttributeType::eSTRING )
            {
                AttributeType::String* strAttr = ( AttributeType::String* )( array::begin( _default_values ) + _offset[i] );
                string::free_and_null( &strAttr->c_str );
            }
        }
        
        
        BX_FREE( bxDefaultAllocator(), _memory_handle );
        _name = nullptr;
        _type = nullptr;
        _offset = nullptr;
        _size = 0;
        _capacity = 0;

        array::clear( _default_values );
        _default_values.~array_t<u8>();
    }

    void setDefaultValue( int index, const void* defaulValue )
    {
        const int numBytes = AttributeType::_stride[_type[index]];
        const u8* defaulValueBytes = (u8*)defaulValue;

        for( int ib = 0; ib < numBytes; ++ib )
        {
            array::push_back( _default_values, defaulValueBytes[ib] );
        }
    }
    void setDefaultValueString( int index, const char* defaultString )
    {
        char* str = string::duplicate( nullptr, defaultString );
        setDefaultValue( index, &str );
    }

    int add( const char* name, AttributeType::Enum typ )
    {
        unsigned nameLen = string::length( name );
        
        SYS_ASSERT( nameLen <= 8 );
        SYS_ASSERT( typ != AttributeType::eTYPE_COUNT );

        if( _size >= _capacity )
        {
            alloc( _capacity * 2 + 4 );
        }

        int index = _size++;
        _type[index] = typ;
        _name[index].hash = 0;
        memcpy( _name[index].str, name, nameLen );

        const int numBytes = AttributeType::_stride[typ];
        const int offset = array::size( _default_values );
        _offset[index] = offset;
        return index;
    }

    AttributeType::Enum type( int index ) const { return _type[index]; }
    int stride( int index ) const { return AttributeType::_stride[_type[index]]; }
    int alignment( int index ) const { return AttributeType::_alignment[_type[index]]; }
};

struct AttributeInstance
{
    struct Value
    {
        u16 type = AttributeType::eTYPE_COUNT;
        u16 offset = 0;
    };

    Value* _values = nullptr;
    u8* _blob = nullptr;
    u32 _blob_size_in_bytes = 0;
    i32 _num_values = 0;

    static void startup( AttributeInstance** outPtr, const AttributeStruct& attrStruct )
    {
        const int n = attrStruct._size;
        if( n == 0 )
        {
            return;
        }

        const int memDataSize = array::size( attrStruct._default_values );
        //for( int i = 0; i < n; ++i )
        //{
        //    memDataSize += attrStruct.stride( i );
        //}

        int memSize = n * sizeof( Value );
        memSize += memDataSize;
        memSize += sizeof( AttributeInstance );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
        memset( mem, 0x00, memSize );
        
        bxBufferChunker chunker( mem, memSize );

        AttributeInstance* attrI = chunker.add< AttributeInstance >();
        attrI->_values = chunker.add< Value >( n );
        attrI->_blob = chunker.addBlock( memDataSize );

        chunker.check();

        attrI->_blob_size_in_bytes = memDataSize;
        attrI->_num_values = n;

        for( int i = 0; i < n; ++i )
        {
            Value& v = attrI->_values[i];
            v.offset = attrStruct._offset[i];
            v.type = attrStruct._type[i];

            const void* defaultValuePtr = array::begin( attrStruct._default_values ) + v.offset;
            if( attrStruct._type[i] != AttributeType::eSTRING )
            {
                attrI->dataSet( i, attrStruct._type[i], defaultValuePtr );
            }
            else
            {
                char** str = (char**)defaultValuePtr;
                attrI->stringSet( i, *str );
            }
        }

        outPtr[0] = attrI;
    }

    static void shutdown( AttributeInstance** attrIPtr )
    {
        AttributeInstance* attrI = attrIPtr[0];
        if( !attrI )
            return;

        for( int i = 0; i < attrI->_num_values; ++i )
        {
            Value& v = attrI->_values[i];
            if( v.type == AttributeType::eSTRING )
            {
                char** str = ( char** )( attrI->_blob + v.offset );
                string::free_and_null( str );
            }
        }

        BX_FREE0( bxDefaultAllocator(), attrIPtr[0] );
    }

    u8* pointerGet( int index, AttributeType::Enum type )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == type );

        return _blob + _values[index].offset;
    }
    void dataSet( int index, AttributeType::Enum type, const void* data )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == type );
        SYS_ASSERT( type != AttributeType::eSTRING );

        if( type == AttributeType::eINT )
        {
            const int datai = *(const int*)data;
            u8* dst = _blob + _values[index].offset;
            int* dstI = (int*)dst;
            dstI[0] = datai;
        }
        else
        {
            const int stride = AttributeType::_stride[type];
            u8* dst = _blob + _values[index].offset;
            memcpy( dst, data, stride );
        }
        
    }
    void stringSet( int index, const char* str )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == AttributeType::eSTRING );

        char** dstStr = ( char** )( _blob + _values[index].offset );
        *dstStr = string::duplicate( *dstStr, str );
    }

};


//////////////////////////////////////////////////////////////////////////
struct NodeType
{
    i32 index = -1;
    const NodeTypeInfo* info = nullptr;
    AttributeStruct attributes;
};

bool nodeInstanceInfoEmpty( const NodeInstanceInfo& info )
{
    bool result = true;
    result &= info.type_index == -1;
    result &= info.id.hash == 0;

    result &= info.type_name == nullptr;
    result &= info.name == nullptr;

    result &= info.graph == nullptr;
    //result &= info._parent.hash == 0;
    //result &= info._first_child.hash == 0;
    //result &= info._next_slibling.hash == 0;
    return result;
}
void nodeInstanceInfoClear( NodeInstanceInfo* info )
{
    memset( info, 0x00, sizeof( NodeInstanceInfo ) );
    info->type_index = -1;
}
void nodeInstanceInfoRelease( NodeInstanceInfo* info )
{
    string::free( (char*)info->name );
    nodeInstanceInfoClear( info );
}

//////////////////////////////////////////////////////////////////////////
struct NodeToDestroy
{
    Node* node = nullptr;
    NodeInstanceInfo* info = nullptr;
    AttributeInstance* attributes = nullptr;
};
struct NodeToUnload
{
    id_t id = makeInvalidHandle<id_t>();
    bool destroyAfterUnload = false;
};
struct GraphToLoad
{
    Graph* graph = nullptr;
    char* filename = nullptr;
};
struct GraphToUnload
{
    Graph* graph = nullptr;
    bool destroyAfterUnload = false;
};

struct GraphContext
{
    enum
    {
        eMAX_NODES = 1024*8,
    };
    array_t< NodeType >         _node_types;
    id_table_t< eMAX_NODES >    _id_table;
    Node*                       _nodes[eMAX_NODES];
    AttributeInstance*          _attributes[eMAX_NODES];
    NodeInstanceInfo*           _instance_info[eMAX_NODES];
    
    bxPoolAllocator             _alloc_instance_info;

    array_t< NodeToDestroy >    _nodes_to_destroy;
    array_t< Graph* >           _graphs;
    queue_t< GraphToLoad >      _graphs_to_load;
    queue_t< GraphToUnload >    _graphs_to_unload;
    queue_t< Graph* >           _graphs_to_destroy;

    bxRecursiveBenaphore        _lock_nodes;
    bxRecursiveBenaphore        _lock_instance_info_alloc;
    bxBenaphore                 _lock_graphs;
    bxBenaphore                 _lock_graphs_to_load;
    bxBenaphore                 _lock_graphs_to_unload;
    bxBenaphore                 _lock_nodes_to_destroy;
    
    void startup()
    {
        memset( _nodes, 0x00, sizeof( _nodes ) );
        memset( _instance_info, 0x00, sizeof( _instance_info ) );
        memset( _attributes, 0x00, sizeof( _attributes ) );
        _alloc_instance_info.startup( sizeof( NodeInstanceInfo ), eMAX_NODES, bxDefaultAllocator(), 8 );
    }

    void shutdown()
    {
        SYS_ASSERT( id_table::size( _id_table ) == 0 );
        for( int i = 0; i < eMAX_NODES; ++i )
        {
            SYS_ASSERT( _nodes[i] == nullptr );
            SYS_ASSERT( _instance_info[i] == nullptr );
        }
        _alloc_instance_info.shutdown();

        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            _node_types[i].info->_interface->typeDeinit();
            //if( _node_types[i].info->_interface->typeInit )
            //{
            //    ( *_node_types[i].info->_type_deinit )( );
            //}

            _node_types[i].attributes.free();
        }
    }

    void nodesLock() { _lock_nodes.lock(); }
    void nodesUnlock() { _lock_nodes.unlock(); }
    void graphsLock() { _lock_graphs.lock(); }
    void graphsUnlock() { _lock_graphs.unlock(); }
        
    //////////////////////////////////////////////////////////////////////////
    NodeInstanceInfo* nodeInstanceInfoAllocate()
    {
        _lock_instance_info_alloc.lock();
        NodeInstanceInfo* info = (NodeInstanceInfo*)_alloc_instance_info.alloc();
        _lock_instance_info_alloc.unlock();
        nodeInstanceInfoClear( info );
        return info;
    }
    void nodeInstanceInfoFree( NodeInstanceInfo* info )
    {
        if( !info )
            return;
        
        nodeInstanceInfoRelease( info );

        _lock_instance_info_alloc.lock();
        _alloc_instance_info.free( info );
        _lock_instance_info_alloc.unlock();
    }

    NodeInstanceInfo* nodeInstanceInfoGet( id_t id )
    {
        return _instance_info[id.index];
    }
    AttributeInstance* attributesInstanceGet( id_t id )
    {
        return _attributes[id.index];
    }

    //////////////////////////////////////////////////////////////////////////
    NodeType* typeGet( int index )
    {
        return &_node_types[index];
    }

    int typeFind( const char* name )
    {
        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            if( string::equal( name, _node_types[i].info->_name ) )
                return i;
        }
        return -1;
    }

    int typeAdd( const NodeTypeInfo* info )
    {
        int found = typeFind( info->_name );
        if( found != -1 )
        {
            bxLogError( "Node type already exists '%s'", info->_name );
            return -1;
        }

        int index = array::push_back( _node_types, NodeType() );
        NodeType& type = array::back( _node_types );
        type.index = index;
        type.info = info;
        //type.info._type_name = string::duplicate( nullptr, info._type_name );

        info->_interface->typeInit( index );
        //if( info->_type_init )
        //{
        //    ( *info->_type_init )( index );
        //}

        return index;
    }


    //////////////////////////////////////////////////////////////////////////
    bool nodeIsValid( id_t id ) const
    {
        return id_table::has( _id_table, id );
    }

    void idAcquire( id_t* id )
    {
        _lock_nodes.lock();
        id[0] = id_table::create( _id_table );
        _lock_nodes.unlock();
    }
    void idRelease( id_t id )
    {
        _lock_nodes.lock();
        id_table::destroy( _id_table, id );
        _lock_nodes.unlock();
    }

    //////////////////////////////////////////////////////////////////////////
    void nodeCreate( id_t id, int typeIndex, const char* nodeName )
    {
        NodeType* type = &_node_types[typeIndex];
        //Node* node = ( *type->info->_creator )( );
        Node* node = type->info->_interface->creator();
        SYS_ASSERT( node != nullptr );

        SYS_ASSERT( _nodes[id.index] == nullptr );
        _nodes[id.index] = node;

        NodeInstanceInfo* instance = nodeInstanceInfoAllocate();
        _instance_info[id.index] = instance;
        instance->type_index = type->index;
        instance->id = id;
        instance->type_name = type->info->_name;
        instance->name = string::duplicate( nullptr, nodeName );

        AttributeInstance::startup( &_attributes[id.index], type->attributes );
    }
    void nodeDestroy( Node* node, NodeInstanceInfo* info, AttributeInstance* attrI )
    {
        const NodeType& type = _node_types[info->type_index];
        SYS_ASSERT( type.index == info->type_index );
        
        AttributeInstance::shutdown( &attrI );

        nodeInstanceInfoFree( info );
        type.info->_interface->destroyer( node );
        //( *type.info->_destroyer )( node );
    }
};
//////////////////////////////////////////////////////////////////////////
static GraphContext* _ctx = nullptr;

//////////////////////////////////////////////////////////////////////////
struct Graph
{
    array_t<id_t>           _id_nodes;
    queue_t<id_t>           _nodes_to_load;
    queue_t<NodeToUnload>   _nodes_to_unload;
    array_t<id_t>           _id_nodes_tick_sort;

    bxBenaphore _lock_nodes;
    bxBenaphore _lock_nodes_to_load;
    bxBenaphore _lock_nodes_to_unload;
    
    std::atomic_uint32_t _flag_recompute;

    void startup()
    {
        _flag_recompute = 0;
    }

    void shutdown( bool destroyNodes )
    {
        for( int i = 0; i < array::size( _id_nodes ); ++i )
        {
            graphNodeRemove( _id_nodes[i], destroyNodes );
        }
    }

    int nodeFind( id_t id )
    {
        return array::find1( array::begin( _id_nodes ), array::end( _id_nodes ), array::OpEqual<id_t>( id ) );
    }


    union NodeSortKey
    {
        u64 hash = 0;
        struct
        {
            u64 index_in_graph : 16;
            u64 instance_index : 16;
            u64 type : 16;
            u64 depth : 16;
        };
    };
    void compile()
    {
        array_t< u64 > sortArray;
        array::reserve( sortArray, array::size( _id_nodes ) );
        for( int i = 0; i < array::size( _id_nodes ); ++i )
        {
            id_t id = _id_nodes[i];
            NodeInstanceInfo info = nodeInstanceInfoGet( id );
            NodeType* type = _ctx->typeGet( info.type_index );
            if( type->info->_exec_mask & EExecMask::eTICK0 )
            //if( type->info->_tick )
            {
                NodeSortKey key;
                key.depth = 1;
                key.type = info.type_index;
                key.instance_index = id.index;
                key.index_in_graph = i;

                array::push_back( sortArray, key.hash );
            }
        }

        std::sort( array::begin( sortArray ), array::end( sortArray ), std::less< u64 >() );

        array::clear( _id_nodes_tick_sort );
        array::reserve( _id_nodes_tick_sort, array::size( sortArray ) );

        for( int i = 0; i < array::size( sortArray ); ++i )
        {
            NodeSortKey key;
            key.hash = sortArray[i];
            array::push_back( _id_nodes_tick_sort, _id_nodes[key.index_in_graph] );
        }
    }
};

//////////////////////////////////////////////////////////////////////////
namespace
{
    template< class T, class Tlock >
    inline bool pop_from_back_with_lock( T* value, queue_t<T>& q, Tlock& lock )
    {
        lock.lock();
        bool e = queue::empty( q );
        if( !e )
        {
            value[0] = queue::back( q );
            queue::pop_back( q );
        }
        lock.unlock();
        return !e;
    }

    void graphContext_destroyNodes()
    {
        bool done = false;
        do
        {
            NodeToDestroy ntd;
            _ctx->_lock_nodes_to_destroy.lock();
            done = array::empty( _ctx->_nodes_to_destroy );
            if( !done )
            {
                ntd = array::back( _ctx->_nodes_to_destroy );
                array::pop_back( _ctx->_nodes_to_destroy );
            }
            _ctx->_lock_nodes_to_destroy.unlock();

            if( !done )
            {
                _ctx->nodeDestroy( ntd.node, ntd.info, ntd.attributes );
            }

        } while( !done );
    }

    void graphContext_unloadGraphs()
    {
        bool done = false;
        do 
        {
            GraphToUnload gtu;
            done = !pop_from_back_with_lock( &gtu, _ctx->_graphs_to_unload, _ctx->_lock_graphs_to_unload );

            if( !done )
            {
                for( int i = 0; i < array::size( gtu.graph->_id_nodes ); ++i )
                {
                    graphNodeRemove( gtu.graph->_id_nodes[i], gtu.destroyAfterUnload );
                }
                queue::push_front( _ctx->_graphs_to_destroy, gtu.graph );
            }

        } while ( !done );
    }
    
    void graphContext_destroyGraphs()
    {
        while( !queue::empty( _ctx->_graphs_to_destroy ) )
        {
            Graph* g = queue::back( _ctx->_graphs_to_destroy );
            queue::pop_back( _ctx->_graphs_to_destroy );

            int found = array::find1( array::begin( _ctx->_graphs ), array::end( _ctx->_graphs ), array::OpEqual<Graph*>( g ) );
            SYS_ASSERT( found != -1 );
            array::erase( _ctx->_graphs, found );

            BX_DELETE0( bxDefaultAllocator(), g );
        }
    }



}
void graphPreTick( Graph* graph, Scene* scene );
void graphTick( Graph* graph, Scene* scene );

void graphContextTick( Scene* scene )
{
    graphContext_unloadGraphs();
    
    _ctx->graphsLock();
    
    for( int i = 0; i < array::size( _ctx->_graphs ); ++i )
    {
        graphPreTick( _ctx->_graphs[i], scene );
    }
    
    graphContext_destroyGraphs();

    _ctx->graphsUnlock();

    graphContext_destroyNodes();

    _ctx->nodesLock();
    _ctx->graphsLock();

    for( int i = 0; i < array::size( _ctx->_graphs ); ++i )
    {
        graphTick( _ctx->_graphs[i], scene );
    }

    _ctx->graphsUnlock();
    _ctx->nodesUnlock();
}

//////////////////////////////////////////////////////////////////////////
void graphCreate( Graph** graph )
{
    Graph* g = BX_NEW( bxDefaultAllocator(), Graph );
    g->startup();

    _ctx->graphsLock();
    array::push_back( _ctx->_graphs, g );
    _ctx->graphsUnlock();

    graph[0] = g;
}
void graphDestroy( Graph** graph, bool destroyNodes )
{
    if( !graph[0] )
        return;

    GraphToUnload gtu;
    gtu.graph = graph[0];
    gtu.destroyAfterUnload = destroyNodes;
    graph[0] = nullptr;

    _ctx->_lock_graphs_to_unload.lock();
    queue::push_front( _ctx->_graphs_to_unload, gtu );
    _ctx->_lock_graphs_to_unload.unlock();

    //graph[0]->shutdown( destroyNodes );
    //BX_DELETE0( bxDefaultAllocator(), graph[0] );
}
void graphLoad( Graph* graph, const char* filename )
{
    (void)graph;
    (void)filename;

    GraphToLoad gtl;
    gtl.graph = graph;
    gtl.filename = string::duplicate( nullptr, filename );

    _ctx->_lock_graphs_to_load.lock();
    queue::push_front( _ctx->_graphs_to_load, gtl );
    _ctx->_lock_graphs_to_load.unlock();
}

namespace
{
    void graph_nodesLoad( Graph* graph, Scene* scene )
    {
        bool done = false;
        bool recomputeGraph = false;
        do
        {
            id_t id;
            done = !pop_from_back_with_lock( &id, graph->_nodes_to_load, graph->_lock_nodes_to_load );

            if( !done && _ctx->nodeIsValid( id ) )
            {
                NodeInstanceInfo info = nodeInstanceInfoGet( id );
                Node* node = nodeInstanceGet( id );
                NodeType* type = _ctx->typeGet( info.type_index );
                type->info->_interface->load( node, info, scene );
                //( *type->info->_load )( node, info, scene );

                graph->_lock_nodes.lock();
                array::push_back( graph->_id_nodes, id );
                graph->_lock_nodes.unlock();

                recomputeGraph = true;
            }

        } while( !done );

        graph->_flag_recompute |= recomputeGraph;
    }
    void graph_nodesUnload( Graph* graph, Scene* scene )
    {
        bool done = false;
        bool recomputeGraph = false;
        do
        {
            NodeToUnload ntu;
            done = !pop_from_back_with_lock( &ntu, graph->_nodes_to_unload, graph->_lock_nodes_to_unload );

            if( !done && _ctx->nodeIsValid( ntu.id ) )
            {
                NodeInstanceInfo* info = _ctx->nodeInstanceInfoGet( ntu.id );
                Node* node = nodeInstanceGet( ntu.id );
                NodeType* type = _ctx->typeGet( info->type_index );
                type->info->_interface->unload( node, *info, scene );
                //( *type->info->_unload )( node, *info, scene );

                info->graph = nullptr;

                graph->_lock_nodes.lock();
                int found = graph->nodeFind( ntu.id );
                SYS_ASSERT( found != -1 );
                array::erase_swap( graph->_id_nodes, found );
                graph->_lock_nodes.unlock();


                if( ntu.destroyAfterUnload )
                {
                    nodeDestroy( &ntu.id );
                }

                recomputeGraph = true;
            }
        } while( !done );

        graph->_flag_recompute |= recomputeGraph;
    }
    void graph_nodesTick( Graph* graph, Scene* scene )
    {
        const int n = array::size( graph->_id_nodes_tick_sort );
        for( int i = 0; i < n; ++i )
        {
            id_t id = graph->_id_nodes_tick_sort[i];

            NodeInstanceInfo info = nodeInstanceInfoGet( id );
            Node* node = nodeInstanceGet( id );
            NodeType* type = _ctx->typeGet( info.type_index );
            type->info->_interface->tick0( node, info, scene );
            //( *type->info->_tick )( node, info, scene );
        }
    }
}
void graphPreTick( Graph* graph, Scene* scene )
{
    graph_nodesLoad( graph, scene );
    graph_nodesUnload( graph, scene );
}

void graphTick( Graph* graph, Scene* scene )
{
    graph->_lock_nodes.lock();

    if( graph->_flag_recompute )
    {
        graph->_flag_recompute = false;
        graph->compile();
    }

    graph_nodesTick( graph, scene );

    graph->_lock_nodes.unlock();
}

bool graphNodeAdd( Graph* graph, id_t id )
{
    bool result = false;

    //graph->_lock_nodes.lock();
    //int found = graph->nodeFind( id );
    //graph->_lock_nodes.unlock();

    //if( found == -1 )
    {
        _ctx->nodesLock();
        NodeInstanceInfo* info = _ctx->nodeInstanceInfoGet( id );
        if( info )
        {
            if( info->graph )
            {
                bxLogError( "Node is already in graph." );
            }
            else
            {
                info->graph = graph;
                result = true;
            }
        }
        _ctx->nodesUnlock();
    }

    if( result )
    {
        //graph->_lock_nodes.lock();
        //array::push_back( graph->_id_nodes, id );
        //graph->_lock_nodes.unlock();

        graph->_lock_nodes_to_load.lock();
        queue::push_front( graph->_nodes_to_load, id );
        graph->_lock_nodes_to_load.unlock();
        //graph->_flag_recompute = true;
    }

    return result;
}
void graphNodeRemove( id_t id, bool destroyNode )
{
    Graph* g = nullptr;

    _ctx->nodesLock();
    NodeInstanceInfo* info = _ctx->nodeInstanceInfoGet( id );
    if( info )
    {
        g = info->graph;
        //info->_graph = nullptr;
    }
    _ctx->nodesUnlock();

    if( g )
    {
        //g->_lock_nodes.lock();
        //int found = g->nodeFind( id );
        //SYS_ASSERT( found != -1 );
        //array::erase_swap( g->_id_nodes, found );
        //g->_lock_nodes.unlock();

        NodeToUnload ntu;
        ntu.id = id;
        ntu.destroyAfterUnload = destroyNode;

        g->_lock_nodes_to_unload.lock();
        queue::push_front( g->_nodes_to_unload, ntu );
        g->_lock_nodes_to_unload.unlock();
        //g->_flag_recompute = true;
    }
}
void graphNodeLink( id_t parent, id_t child )
{
}

void graphNodeUnlink( id_t child )
{
}


//////////////////////////////////////////////////////////////////////////
bool nodeRegister( const NodeTypeInfo* typeInfo )
{
    int ires = _ctx->typeAdd( typeInfo );
    return ( ires == -1 ) ? false : true;
}

bool nodeCreate( id_t* out, const char* typeName, const char* nodeName )
{
    int typeIndex = _ctx->typeFind( typeName );
    if( typeIndex == -1 )
    {
        bxLogError( "Node type '%s' not found", typeName );
        return false;
    }

    _ctx->idAcquire( out );
    _ctx->nodeCreate( *out, typeIndex, nodeName );
    
    return _ctx->nodeIsValid( *out );
}
void nodeDestroy( id_t* inOut )
{
    if( !_ctx->nodeIsValid( *inOut ) )
        return;

    NodeToDestroy ntd;
    ntd.node = _ctx->_nodes[inOut->index];
    ntd.info = _ctx->_instance_info[ inOut->index ];
    ntd.attributes = _ctx->_attributes[inOut->index];

    _ctx->_nodes[inOut->index] = nullptr;
    _ctx->_instance_info[inOut->index] = nullptr;
    _ctx->_attributes[inOut->index] = nullptr;
    
    _ctx->idRelease( *inOut );
    inOut[0] = makeInvalidHandle<id_t>();

    SYS_ASSERT( ntd.info->graph == nullptr );

    _ctx->_lock_nodes_to_destroy.lock();
    array::push_back( _ctx->_nodes_to_destroy, ntd );
    _ctx->_lock_nodes_to_destroy.unlock();
}

bool nodeIsAlive( id_t id )
{
    return _ctx->nodeIsValid( id );
}

NodeInstanceInfo nodeInstanceInfoGet( id_t id )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    return *_ctx->_instance_info[id.index];
}

Node* nodeInstanceGet( id_t id )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    return _ctx->_nodes[id.index];
}
////
//
namespace
{
    AttributeIndex nodeAttributeAdd( int typeIndex, const char* name, AttributeType::Enum attributeType, const void* defaultValue )
    {
        NodeType* type = _ctx->typeGet( typeIndex );
        SYS_ASSERT( type->attributes.find( name ) == -1 );
        AttributeIndex index = type->attributes.add( name, attributeType );
        type->attributes.setDefaultValue( index, defaultValue );
        return index;
    }

    NodeType* nodeTypeGet( id_t id )
    {
        SYS_ASSERT( _ctx->nodeIsValid( id ) );
        NodeInstanceInfo* info = _ctx->nodeInstanceInfoGet( id );
        NodeType* type = _ctx->typeGet( info->type_index );
        return type;
    }
}
AttributeIndex nodeAttributeAddFloat( int typeIndex, const char* name, float defaultValue )
{
    return nodeAttributeAdd( typeIndex, name, AttributeType::eFLOAT, &defaultValue );
}
AttributeIndex nodeAttributeAddInt( int typeIndex, const char* name, int defaultValue )
{
    return nodeAttributeAdd( typeIndex, name, AttributeType::eINT, &defaultValue );
}
AttributeIndex nodeAttributeAddFloat3( int typeIndex, const char* name, const float3_t& defaultValue )
{
    return nodeAttributeAdd( typeIndex, name, AttributeType::eFLOAT3, &defaultValue );
}
AttributeIndex nodeAttributeAddString( int typeIndex, const char* name, const char* defaultValue )
{
    NodeType* type = _ctx->typeGet( typeIndex );
    SYS_ASSERT( type->attributes.find( name ) == -1 );

    AttributeIndex index = type->attributes.add( name, AttributeType::eSTRING );
    type->attributes.setDefaultValueString( index, defaultValue );

    return index;
}


bool nodeAttributeFloat( float* out, id_t id, const char* name )
{
    NodeType* type = nodeTypeGet( id );
    int index = type->attributes.find( name );
    if( index != -1 )
    {
        out[0] = nodeAttributeFloat( id, index );
        return true;
    }

    return false;
}
bool nodeAttributeInt( int* out, id_t id, const char* name )
{
    NodeType* type = nodeTypeGet( id );
    int index = type->attributes.find( name );
    if( index != -1 )
    {
        out[0] = nodeAttributeInt( id, index );
        return true;
    }

    return false;
}

bool nodeAttributeVector3( Vector3* out, id_t id, const char* name )
{
    NodeType* type = nodeTypeGet( id );
    int index = type->attributes.find( name );
    if( index != -1 )
    {
        out[0] = nodeAttributeVector3( id, index );
        return true;
    }

    return false;
}

bool nodeAttributeString( const char** out, id_t id, const char* name )
{
    NodeType* type = nodeTypeGet( id );
    int index = type->attributes.find( name );
    if( index != -1 )
    {
        out[0] = nodeAttributeString( id, index );
        return true;
    }
    return false;
}

float nodeAttributeFloat( id_t id, AttributeIndex index )
{
    float* ptr = (float*)_ctx->attributesInstanceGet( id )->pointerGet( index, AttributeType::eFLOAT );
    return *ptr;
}
int nodeAttributeInt( id_t id, AttributeIndex index )
{
    int* ptr = (int*)_ctx->attributesInstanceGet( id )->pointerGet( index, AttributeType::eINT );
    return *ptr;
}
Vector3 nodeAttributeVector3( id_t id, AttributeIndex index )
{
    float3_t* ptr = (float3_t*)_ctx->attributesInstanceGet( id )->pointerGet( index, AttributeType::eFLOAT3 );
    return Vector3( ptr->x, ptr->y, ptr->z );
}
const char* nodeAttributeString( id_t id, AttributeIndex index )
{
    AttributeType::String* ptr = ( AttributeType::String* )_ctx->attributesInstanceGet( id )->pointerGet( index, AttributeType::eSTRING );
    return ptr->c_str;
}


namespace
{
    bool nodeAttributeSetByName( id_t id, const char* name, AttributeType::Enum type, const void* data )
    {
        NodeType* nodeType = nodeTypeGet( id );
        int index = nodeType->attributes.find( name );
        if( index != -1 )
        {
            _ctx->attributesInstanceGet( id )->dataSet( index, type, data );
            return true;
        }
        return false;
    }

    bool nodeAttributeSetByName1( id_t id, const char* name, const void* data, unsigned dataSize )
    {
        bool result = false;
        NodeType* nodeType = nodeTypeGet( id );
        int index = nodeType->attributes.find( name );
        if( index != -1 )
        {
            AttributeType::Enum type = nodeType->attributes.type( index );
            SYS_ASSERT( type != AttributeType::eTYPE_COUNT );

            if( type != AttributeType::eSTRING )
            {
                const unsigned stride = nodeType->attributes.stride( index );
                if( stride <= dataSize )
                {
                    _ctx->attributesInstanceGet( id )->dataSet( index, type, data );
                    result = true;
                }
            }
            else
            {
                _ctx->attributesInstanceGet( id )->stringSet( index, (const char*)data );
                result = true;
            }
        }
        return result;
    }
}///

bool nodeAttributeFloatSet( id_t id, const char* name, float value )
{
    return nodeAttributeSetByName( id, name, AttributeType::eFLOAT, &value );
}
bool nodeAttributeIntSet( id_t id, const char* name, int value )
{
    return nodeAttributeSetByName( id, name, AttributeType::eINT, &value );
}
bool nodeAttributeVector3Set( id_t id, const char* name, const Vector3 value )
{
    return nodeAttributeSetByName( id, name, AttributeType::eFLOAT3, &value );
}
bool nodeAttributeStringSet( id_t id, const char* name, const char* value )
{
    NodeType* type = nodeTypeGet( id );
    int index = type->attributes.find( name );
    if( index != -1 )
    {
        _ctx->attributesInstanceGet( id )->stringSet( index, value );
        return true;
    }
    return false;
}

void nodeAttributeFloatSet( id_t id, AttributeIndex index, float value )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    _ctx->attributesInstanceGet( id )->dataSet( index, AttributeType::eFLOAT, &value );
}
void nodeAttributeIntSet( id_t id, AttributeIndex index, int value )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    _ctx->attributesInstanceGet( id )->dataSet( index, AttributeType::eINT, &value );
}
void nodeAttributeVector3Set( id_t id, AttributeIndex index, const Vector3 value )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    _ctx->attributesInstanceGet( id )->dataSet( index, AttributeType::eFLOAT3, &value );
}
void nodeAttributeStringSet( id_t id, AttributeIndex index, const char* value )
{
    SYS_ASSERT( _ctx->nodeIsValid( id ) );
    _ctx->attributesInstanceGet( id )->stringSet( index, value );
}


void GraphSceneScriptCallback::addCallback( bxAsciiScript* script )
{
    for( int i = 0; i < array::size( _ctx->_node_types ); ++i )
    {
        const char* typeName = _ctx->_node_types[i].info->_name;
        bxScene::script_addCallback( script, typeName, this );
    }
    
    _current_id = makeInvalidHandle<id_t>();
}

void GraphSceneScriptCallback::onCreate( const char* typeName, const char* objectName )
{
    bool bres = nodeCreate( &_current_id, typeName, objectName );
    if( bres )
    {
        bres = graphNodeAdd( _graph, _current_id );
        if( !bres )
        {
            SYS_NOT_IMPLEMENTED;
        }
    }
    else
    {
        _current_id = makeInvalidHandle<id_t>();
        bxLogError( "Failed to create node '%s:%s", typeName, objectName );
    }
}

void GraphSceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
{
    if( !nodeIsAlive( _current_id ) )
        return;

    bool bres = nodeAttributeSetByName1( _current_id, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
    if( !bres )
    {
        bxLogError( "node attribute '%s' set failed", attrName );
    }
}

void GraphSceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
{
    (void)cmdName;
    (void)args;
}

//////////////////////////////////////////////////////////////////////////
void graphContextStartup()
{
    SYS_ASSERT( _ctx == nullptr );
    _ctx = BX_NEW( bxDefaultAllocator(), GraphContext );
    _ctx->startup();
}
void graphContextShutdown()
{
    if( !_ctx )
        return;

    _ctx->shutdown();
    BX_DELETE0( bxDefaultAllocator(), _ctx );
}

void graphContextCleanup( Scene* scene )
{
    for( int i = 0; i < array::size( _ctx->_graphs ); ++i )
    {
        Graph* g = _ctx->_graphs[i];
        graphDestroy( &g, true );
    }

    graphContextTick( scene );
}

}////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#include <gfx/gfx.h>
namespace bx
{
    BX_GRAPH_DEFINE_NODE( LocatorNode, LocatorNode::Interface );
    BX_LOCATORNODE_ATTRIBUTES_DEFINE

    void LocatorNode::Interface::typeInit( int typeIndex )
    {
        BX_LOCATORNODE_ATTRIBUTES_CREATE
    }

    void LocatorNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

        self( node )->_pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    }

    //void LocatorNode::_TypeInit( int typeIndex )
    //{
    //    BX_LOCATORNODE_ATTRIBUTES_CREATE
    //}
    //void LocatorNode::_TypeDeinit()
    //{}
    //Node* LocatorNode::_Creator()
    //{
    //    return BX_NEW( bxDefaultAllocator(), LocatorNode );
    //}
    //void LocatorNode::_Destroyer( Node* node )
    //{
    //    BX_DELETE( bxDefaultAllocator(), node );
    //}
    //void LocatorNode::_Load( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
    //    const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
    //    const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

    //    self( node )->_pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    //}
    //void LocatorNode::_Unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //}
    //void LocatorNode::tick( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //}

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    BX_GRAPH_DEFINE_NODE( MeshNode, MeshNode::Interface );
    BX_MESHNODE_ATTRIBUTES_DEFINE

    void MeshNode::Interface::typeInit( int typeIndex )
    {
        BX_MESHNODE_ATTRIBUTES_CREATE
    }

    void MeshNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        GfxMeshInstance* mi = nullptr;
        gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

        const char* mesh = nodeAttributeString( instance.id, attr_mesh );
        bxGdiRenderSource* rsource = gfxGlobalResourcesGet()->mesh.box;
        if( mesh[0] == ':' )
        {
            if( string::equal( mesh, ":box" ) )
                rsource = gfxGlobalResourcesGet()->mesh.box;
            else if( string::equal( mesh, ":sphere" ) )
                rsource = gfxGlobalResourcesGet()->mesh.sphere;
        }

        const char* material = nodeAttributeString( instance.id, attr_material );
        bxGdiShaderFx_Instance* fxI = gfxMaterialFind( material );
        if( !fxI )
        {
            fxI = gfxMaterialFind( "red" );
        }

        GfxMeshInstanceData miData;
        miData.renderSourceSet( rsource );
        miData.fxInstanceSet( fxI );
        miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );
        gfxMeshInstanceDataSet( mi, miData );

        const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

        Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
        gfxMeshInstanceWorldMatrixSet( mi, &pose, 1 );
        gfxSceneMeshInstanceAdd( scene->gfx, mi );

        self( node )->_mesh_instance = mi;
    }

    void MeshNode::Interface::unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        auto meshNode = self( node );
        gfxMeshInstanceDestroy( &meshNode->_mesh_instance );
    }


    //void MeshNode::_TypeInit( int typeIndex )
    //{
    //    BX_MESHNODE_ATTRIBUTES_CREATE
    //}

    //void MeshNode::_TypeDeinit()
    //{}

    //Node* MeshNode::_Creator()
    //{
    //    return BX_NEW( bxDefaultAllocator(), MeshNode );
    //}

    //void MeshNode::_Destroyer( Node* node )
    //{
    //    BX_DELETE( bxDefaultAllocator(), node );
    //}

    //void MeshNode::_Load( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    GfxMeshInstance* mi = nullptr;
    //    gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

    //    const char* mesh = nodeAttributeString( instance.id, attr_mesh );
    //    bxGdiRenderSource* rsource = gfxGlobalResourcesGet()->mesh.box;
    //    if( mesh[0] == ':' )
    //    {
    //        if( string::equal( mesh, ":box" ) )
    //            rsource = gfxGlobalResourcesGet()->mesh.box;
    //        else if( string::equal( mesh, ":sphere" ) )
    //            rsource = gfxGlobalResourcesGet()->mesh.sphere;
    //    }

    //    const char* material = nodeAttributeString( instance.id, attr_material );
    //    bxGdiShaderFx_Instance* fxI = gfxMaterialFind( material );
    //    if( !fxI )
    //    {
    //        fxI = gfxMaterialFind( "red" );
    //    }
    //           
    //    GfxMeshInstanceData miData;
    //    miData.renderSourceSet( rsource );
    //    miData.fxInstanceSet( fxI );
    //    miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );
    //    gfxMeshInstanceDataSet( mi, miData );

    //    const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos);
    //    const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot);
    //    const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

    //    Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    //    gfxMeshInstanceWorldMatrixSet( mi, &pose, 1 );
    //    gfxSceneMeshInstanceAdd( scene->gfx, mi );

    //    self(node)->_mesh_instance = mi;
    //}

    //void MeshNode::_Unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    auto meshNode = self( node );
    //    gfxMeshInstanceDestroy( &meshNode->_mesh_instance );
    //}

    //void MeshNode::tick( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //
    //}



    


}////
