#include "engine.h"
#include <system/window.h>
#include <system/input.h>

#include <util/config.h>
#include <util/string_util.h>
#include <util/array.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include <util/array_util.h>
#include <util/pool_allocator.h>

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

    rmt_CreateGlobalInstance( &e->_remotery );

    e->_camera_script_callback = BX_NEW( bxDefaultAllocator(), CameraManagerSceneScriptCallback );
    e->_camera_script_callback->_gfx = e->gfx_context;
    e->_camera_script_callback->_menago = e->camera_manager;
    e->_camera_script_callback->_current = nullptr;
}
void Engine::shutdown( Engine* e )
{
    BX_DELETE0( bxDefaultAllocator(), e->_camera_script_callback );
    
    rmt_DestroyGlobalInstance( e->_remotery );
    e->_remotery = 0;

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

struct NodeType
{
    i32 index = -1;
    NodeTypeInfo info;

    ~NodeType()
    {
        string::free_and_null( (char**)&info._type_name );
    }
};
bool nodeInstanceInfoEmpty( const NodeInstanceInfo& info )
{
    bool result = true;
    result &= info._type_index == -1;
    result &= info._instance_id.hash == 0;

    result &= info._type_name == nullptr;
    result &= info._instance_name == nullptr;

    result &= info._graph == nullptr;
    //result &= info._parent.hash == 0;
    //result &= info._first_child.hash == 0;
    //result &= info._next_slibling.hash == 0;
    return result;
}
void nodeInstanceInfoClear( NodeInstanceInfo* info )
{
    memset( info, 0x00, sizeof( NodeInstanceInfo ) );
    info->_type_index = -1;
}
void nodeInstanceInfoRelease( NodeInstanceInfo* info )
{
    string::free( (char*)info->_instance_name );
    nodeInstanceInfoClear( info );
}

//////////////////////////////////////////////////////////////////////////
template< typename T, typename Tlock >
struct array_thread_safe_t : public array_t< T >
{
    Tlock _lock;

    int push_back_with_lock( const T& value )
    {
        lock();
        int index = array::push_back( *this, value );
        unlock();
        return index;
    }
    void erase_swap_with_lock( int index )
    {
        lock();
        array::erase_swap( *this, index );
        unlock();
    }
    void erase_with_lock( int index )
    {
        lock();
        array::erase( *this, index );
        unlock();
    }

    bool pop_from_back_with_lock( T* value )
    {
        lock();
        bool e = array::empty( *this );
        if( !e )
        {
            value[0] = array::back( *this );
            array::pop_back( *this );
        }
        unlock();
        return !e;
    }

    void lock() { _lock.lock(); }
    void unlock() { _lock.unlock(); }
};

struct NodeToDestroy
{
    Node* node = nullptr;
    NodeInstanceInfo* info = nullptr;
};

struct GraphGlobal
{
    enum
    {
        eMAX_NODES = 1024*8,
    };
    array_t< NodeType >         _node_types;
    id_table_t< eMAX_NODES >    _id_table;
    Node*                       _nodes[eMAX_NODES];
    NodeInstanceInfo*           _instance_info[eMAX_NODES];
    bxPoolAllocator             _alloc_instance_info;

    array_t< NodeToDestroy >    _nodes_to_destroy;
    array_t< Graph* >           _graphs;

    bxRecursiveBenaphore        _lock_nodes;
    bxRecursiveBenaphore        _lock_instance_info_alloc;
    bxBenaphore                 _lock_graphs;
    bxBenaphore                 _lock_nodes_to_destroy;
    
    void startup()
    {
        memset( _nodes, 0x00, sizeof( _nodes ) );
        memset( _instance_info, 0x00, sizeof( _instance_info ) );
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
            if( _node_types[i].info._type_deinit )
            {
                ( *_node_types[i].info._type_deinit )( );
            }
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

    //////////////////////////////////////////////////////////////////////////
    NodeType* typeGet( int index )
    {
        return &_node_types[index];
    }

    int typeFind( const char* name )
    {
        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            if( string::equal( name, _node_types[i].info._type_name ) )
                return i;
        }
        return -1;
    }

    int typeAdd( const NodeTypeInfo& info )
    {
        int found = typeFind( info._type_name );
        if( found != -1 )
        {
            bxLogError( "Node type already exists '%s'", info._type_name );
            return -1;
        }
            

        int index = array::push_back( _node_types, NodeType() );
        NodeType& type = array::back( _node_types );
        type.index = index;
        type.info = info;
        type.info._type_name = string::duplicate( nullptr, info._type_name );

        if( info._type_init )
        {
            ( *info._type_init )( );
        }

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
        Node* node = ( *type->info._creator )( );
        SYS_ASSERT( node != nullptr );

        SYS_ASSERT( _nodes[id.index] == nullptr );
        _nodes[id.index] = node;

        NodeInstanceInfo* instance = nodeInstanceInfoAllocate();
        _instance_info[id.index] = instance;
        instance->_type_index = type->index;
        instance->_instance_id = id;
        instance->_type_name = type->info._type_name;
        instance->_instance_name = string::duplicate( nullptr, nodeName );
    }

    /// this function doesn't have to be thread safe. It's called in safe place.
    void nodeDestroy( Node* node, NodeInstanceInfo* info )
    {
        const NodeType& type = _node_types[info->_type_index];
        SYS_ASSERT( type.index == info->_type_index );

        nodeInstanceInfoFree( info );
        ( *type.info._destroyer )( node );
    }
};
static GraphGlobal* _global = nullptr;

//////////////////////////////////////////////////////////////////////////
void graphGlobalStartup()
{
    SYS_ASSERT( _global == nullptr );
    _global = BX_NEW( bxDefaultAllocator(), GraphGlobal );
    _global->startup();
}
void graphGlobalShutdown()
{
    if( !_global )
        return;

    _global->shutdown();
    BX_DELETE0( bxDefaultAllocator(), _global );
}

//////////////////////////////////////////////////////////////////////////
namespace
{
    void graphGlobal_destroyNodes()
    {
        bool done = false;
        do
        {
            NodeToDestroy ntd;
            _global->_lock_nodes_to_destroy.lock();
            done = array::empty( _global->_nodes_to_destroy );
            if( !done )
            {
                ntd = array::back( _global->_nodes_to_destroy );
                array::pop_back( _global->_nodes_to_destroy );
            }
            _global->_lock_nodes_to_destroy.unlock();

            if( !done )
            {
                _global->nodeDestroy( ntd.node, ntd.info );
            }

        } while( !done );
    }
}

void graphGlobalTick( Scene* scene )
{
    graphGlobal_destroyNodes();
}

//////////////////////////////////////////////////////////////////////////
bool nodeRegister( const NodeTypeInfo& typeInfo )
{
    int ires = _global->typeAdd( typeInfo );
    return ( ires == -1 ) ? false : true;
}
//////////////////////////////////////////////////////////////////////////
bool nodeCreate( id_t* out, const char* typeName, const char* nodeName )
{
    int typeIndex = _global->typeFind( typeName );
    if( typeIndex == -1 )
    {
        bxLogError( "Node type '%s' not found", typeName );
        return false;
    }

    _global->idAcquire( out );
    _global->nodeCreate( *out, typeIndex, nodeName );
    
    return _global->nodeIsValid( *out );
}
void nodeDestroy( id_t* inOut )
{
    if( !_global->nodeIsValid( *inOut ) )
        return;

    NodeToDestroy ntd;
    ntd.node = _global->_nodes[inOut->index];
    ntd.info = _global->_instance_info[ inOut->index ];

    _global->_nodes[inOut->index] = nullptr;
    _global->_instance_info[inOut->index] = nullptr;
    
    _global->idRelease( *inOut );
    inOut[0] = makeInvalidHandle<id_t>();

    _global->_lock_nodes_to_destroy.lock();
    array::push_back( _global->_nodes_to_destroy, ntd );
    _global->_lock_nodes_to_destroy.unlock();
}

bool nodeIsAlive( id_t id )
{
    return _global->nodeIsValid( id );
}
//////////////////////////////////////////////////////////////////////////
NodeInstanceInfo nodeInstanceInfoGet( id_t id )
{
    SYS_ASSERT( _global->nodeIsValid( id ) );
    return *_global->_instance_info[id.index];
}

Node* nodeInstanceGet( id_t id )
{
    SYS_ASSERT( _global->nodeIsValid( id ) );
    return _global->_nodes[id.index];
}

//////////////////////////////////////////////////////////////////////////
struct Graph
{
    typedef array_thread_safe_t< id_t, bxBenaphore > IdArrayThreadSafe;
    IdArrayThreadSafe _id_nodes;
    IdArrayThreadSafe _nodes_to_load;
    IdArrayThreadSafe _nodes_to_unload;
    
    array_t< id_t > _id_nodes_tick_sort;

    std::atomic_bool _flag_recompute;

    void startup()
    {
        _flag_recompute = false;
    }
    void shutdown( bool destroyNodes )
    {
        for( int i = 0; i < array::size( _id_nodes ); ++i )
        {
            graphNodeRemove( _id_nodes[i] );
        }
        if( destroyNodes )
        {
            for( int i = 0; i < array::size( _id_nodes ); ++i )
            {
                nodeDestroy( &_id_nodes[i] );
            }
        }

        array::clear( _id_nodes );
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

            NodeSortKey key;
            key.depth = 1;
            key.type = info._type_index;
            key.instance_index = id.index;
            key.index_in_graph = i;

            array::push_back( sortArray, key.hash );
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

void graphCreate( Graph** graph )
{
    Graph* g = BX_NEW( bxDefaultAllocator(), Graph );
    g->startup();

    _global->graphsLock();
    array::push_back( _global->_graphs, g );
    _global->graphsUnlock();

    graph[0] = g;
}
void graphDestroy( Graph** graph, bool destroyNodes )
{
    if( !graph[0] )
        return;

    _global->graphsLock();
    int found = array::find1( array::begin( _global->_graphs ), array::end( _global->_graphs ), array::OpEqual<Graph*>( graph[0] ) );
    SYS_ASSERT( found != -1 );
    array::erase( _global->_graphs, found );
    _global->graphsUnlock();

    graph[0]->shutdown( destroyNodes );
    BX_DELETE0( bxDefaultAllocator(), graph[0] );
}

namespace
{
    void graph_nodesLoad( Graph* graph, Scene* scene )
    {
        bool done = false;
        do 
        {
            id_t id;
            done = !graph->_nodes_to_load.pop_from_back_with_lock( &id );

            if( !done && _global->nodeIsValid( id ) )
            {
                NodeInstanceInfo info = nodeInstanceInfoGet( id );
                Node* node = nodeInstanceGet( id );
                NodeType* type = _global->typeGet( info._type_index );
                ( *type->info._load )( node, info, scene );
            }

        } while ( !done );
    }
    void graph_nodesUnload( Graph* graph, Scene* scene )
    {
        bool done = false;
        do
        {
            id_t id;
            done = !graph->_nodes_to_unload.pop_from_back_with_lock( &id );

            if( !done && _global->nodeIsValid( id ) )
            {
                NodeInstanceInfo info = nodeInstanceInfoGet( id );
                Node* node = nodeInstanceGet( id );
                NodeType* type = _global->typeGet( info._type_index );
                ( *type->info._unload )( node, info, scene );
            }
        } while( !done );
    }
    void graph_nodesTick( Graph* graph, Scene* scene )
    {
        const int n = array::size( graph->_id_nodes_tick_sort );
        for( int i = 0; i < n; ++i )
        {
            id_t id = graph->_id_nodes_tick_sort[i];

            NodeInstanceInfo info = nodeInstanceInfoGet( id );
            Node* node = nodeInstanceGet( id );
            NodeType* type = _global->typeGet( info._type_index );
            ( *type->info._tick )( node, info, scene );
        }
    }
}

void graphTick( Graph* graph, Scene* scene )
{
    graph->_id_nodes.lock();

    graph_nodesLoad( graph, scene );
    graph_nodesUnload( graph, scene );

    if( graph->_flag_recompute )
    {
        graph->_flag_recompute = false;
        graph->compile();
    }
    
    graph_nodesTick( graph, scene );

    graph->_id_nodes.unlock();
}

bool graphNodeAdd( Graph* graph, id_t id )
{
    bool result = false;

    graph->_id_nodes.lock();
    int found = graph->nodeFind( id );
    graph->_id_nodes.unlock();

    if( found == -1 )
    {
        _global->nodesLock();
        NodeInstanceInfo* info = _global->nodeInstanceInfoGet( id );
        if( info )
        {
            if( info->_graph )
            {
                bxLogError( "Node is already in graph." );
            }
            else
            {
                info->_graph = graph;
                result = true;
            }
        }
        _global->nodesUnlock();
    }

    if( result )
    {
        graph->_id_nodes.push_back_with_lock( id );
        graph->_nodes_to_load.push_back_with_lock( id );
        graph->_flag_recompute = true;
    }
    
    return result;
}
void graphNodeRemove( id_t id )
{
    Graph* g = nullptr;

    _global->nodesLock();
    NodeInstanceInfo* info = _global->nodeInstanceInfoGet( id );
    if( info )
    {
        g = info->_graph;
        info->_graph = nullptr;
    }
    _global->nodesUnlock();

    if( g )
    {
        g->_id_nodes.lock();
        int found = g->nodeFind( id );
        SYS_ASSERT( found != -1 );
        array::erase_swap( g->_id_nodes, found );
        g->_id_nodes.unlock();

        g->_nodes_to_unload.push_back_with_lock( id );
        g->_flag_recompute = true;
    }
}
void graphNodeLink( id_t parent, id_t child )
{
}

void graphNodeUnlink( id_t child )
{
}




}////