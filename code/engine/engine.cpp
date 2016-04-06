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
 
    graphContextStartup();

    rmt_CreateGlobalInstance( &e->_remotery );
}
void Engine::shutdown( Engine* e )
{
    rmt_DestroyGlobalInstance( e->_remotery );
    e->_remotery = 0;

    graphContextShutdown();

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

struct NodeType
{
    i32 index = -1;
    const NodeTypeInfo* info = nullptr;

    //~NodeType()
    //{
    //    string::free_and_null( (char**)&info._type_name );
    //}
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
struct NodeToDestroy
{
    Node* node = nullptr;
    NodeInstanceInfo* info = nullptr;
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
            if( _node_types[i].info->_type_deinit )
            {
                ( *_node_types[i].info->_type_deinit )( );
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
            if( string::equal( name, _node_types[i].info->_type_name ) )
                return i;
        }
        return -1;
    }

    int typeAdd( const NodeTypeInfo* info )
    {
        int found = typeFind( info->_type_name );
        if( found != -1 )
        {
            bxLogError( "Node type already exists '%s'", info->_type_name );
            return -1;
        }

        int index = array::push_back( _node_types, NodeType() );
        NodeType& type = array::back( _node_types );
        type.index = index;
        type.info = info;
        //type.info._type_name = string::duplicate( nullptr, info._type_name );

        if( info->_type_init )
        {
            ( *info->_type_init )( );
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
        Node* node = ( *type->info->_creator )( );
        SYS_ASSERT( node != nullptr );

        SYS_ASSERT( _nodes[id.index] == nullptr );
        _nodes[id.index] = node;

        NodeInstanceInfo* instance = nodeInstanceInfoAllocate();
        _instance_info[id.index] = instance;
        instance->_type_index = type->index;
        instance->_instance_id = id;
        instance->_type_name = type->info->_type_name;
        instance->_instance_name = string::duplicate( nullptr, nodeName );
    }
    void nodeDestroy( Node* node, NodeInstanceInfo* info )
    {
        const NodeType& type = _node_types[info->_type_index];
        SYS_ASSERT( type.index == info->_type_index );

        nodeInstanceInfoFree( info );
        ( *type.info->_destroyer )( node );
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
            NodeType* type = _ctx->typeGet( info._type_index );
            if( type->info->_tick )
            {
                NodeSortKey key;
                key.depth = 1;
                key.type = info._type_index;
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

    void graphGlobal_destroyNodes()
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
                _ctx->nodeDestroy( ntd.node, ntd.info );
            }

        } while( !done );
    }

    void graphGlobal_unloadGraphs()
    {
        bool done = false;
        do 
        {
            GraphToUnload gtu;
            done = !pop_from_back_with_lock( &gtu, _ctx->_graphs_to_unload, _ctx->_lock_graphs_to_unload );

            if( !done )
            {
                gtu.graph->shutdown( gtu.destroyAfterUnload );
                queue::push_front( _ctx->_graphs_to_destroy, gtu.graph );
            }

        } while ( !done );
    }
    
    void graphGlobal_destroyGraphs()
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
    graphGlobal_unloadGraphs();
    
    _ctx->graphsLock();
    
    for( int i = 0; i < array::size( _ctx->_graphs ); ++i )
    {
        graphPreTick( _ctx->_graphs[i], scene );
    }
    
    graphGlobal_destroyGraphs();

    _ctx->graphsUnlock();

    graphGlobal_destroyNodes();

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
bool nodeRegister( const NodeTypeInfo* typeInfo )
{
    int ires = _ctx->typeAdd( typeInfo );
    return ( ires == -1 ) ? false : true;
}
//////////////////////////////////////////////////////////////////////////
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

    _ctx->_nodes[inOut->index] = nullptr;
    _ctx->_instance_info[inOut->index] = nullptr;
    
    _ctx->idRelease( *inOut );
    inOut[0] = makeInvalidHandle<id_t>();

    SYS_ASSERT( ntd.info->_graph == nullptr );

    _ctx->_lock_nodes_to_destroy.lock();
    array::push_back( _ctx->_nodes_to_destroy, ntd );
    _ctx->_lock_nodes_to_destroy.unlock();
}

bool nodeIsAlive( id_t id )
{
    return _ctx->nodeIsValid( id );
}
//////////////////////////////////////////////////////////////////////////
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
                NodeType* type = _ctx->typeGet( info._type_index );
                ( *type->info->_load )( node, info, scene );

                graph->_lock_nodes.lock();
                array::push_back( graph->_id_nodes, id );
                graph->_lock_nodes.unlock();

                recomputeGraph = true;
            }

        } while ( !done );

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
                NodeType* type = _ctx->typeGet( info->_type_index );
                ( *type->info->_unload )( node, *info, scene );

                info->_graph = nullptr;
                
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
            NodeType* type = _ctx->typeGet( info._type_index );
            ( *type->info->_tick )( node, info, scene );
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
        g = info->_graph;
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




}////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#include <gfx/gfx.h>
namespace bx
{

    NodeTypeInfo MeshNode::__type_info = MeshNode::__typeInfoFill();
    void MeshNode::_TypeInit()
    {

    }

    void MeshNode::_TypeDeinit()
    {

    }

    Node* MeshNode::_Creator()
    {
        return BX_NEW( bxDefaultAllocator(), MeshNode );
    }

    void MeshNode::_Destroyer( Node* node )
    {
        BX_FREE( bxDefaultAllocator(), node );
    }

    void MeshNode::_Load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        GfxMeshInstance* mi = nullptr;
        gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

        GfxMeshInstanceData miData;
        miData.renderSourceSet( gfxGlobalResourcesGet()->mesh.box );
        miData.fxInstanceSet( gfxMaterialFind( "red" ) );
        miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );
        gfxMeshInstanceDataSet( mi, miData );
        gfxMeshInstanceWorldMatrixSet( mi, &Matrix4::identity(), 1 );
        gfxSceneMeshInstanceAdd( scene->gfx, mi );

        self(node)->_mesh_instance = mi;
    }

    void MeshNode::_Unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        auto meshNode = self( node );
        gfxMeshInstanceDestroy( &meshNode->_mesh_instance );
    }
}////
