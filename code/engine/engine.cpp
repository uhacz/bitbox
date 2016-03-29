#include "engine.h"
#include <system/window.h>
#include <system/input.h>

#include <util/config.h>
#include <util/string_util.h>
#include <util/array.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>

#include <gfx/gfx_gui.h>
#include <gfx/gfx_debug_draw.h>

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
    i32 id = -1;
    NodeTypeInfo info;

    ~NodeType()
    {
        string::free_and_null( (char**)&info._type_name );
    }
};
bool nodeInstanceInfoEmpty( const NodeInstanceInfo& info )
{
    bool result = true;
    result &= info._type_id == -1;
    result &= info._instance_id.hash == 0;

    result &= info._type_name == nullptr;
    result &= info._instance_name == nullptr;

    result &= info._graph == nullptr;
    result &= info._parent.hash == 0;
    result &= info._first_child.hash == 0;
    result &= info._next_slibling.hash == 0;
    return result;
}
void nodeInstanceInfoClear( NodeInstanceInfo* info )
{
    memset( info, 0x00, sizeof( NodeInstanceInfo ) );
    info->_type_id = -1;
}


struct GraphGlobal
{
    enum
    {
        eMAX_NODES = 1024*8,
    };
    array_t< NodeType > _node_types;
    id_table_t< eMAX_NODES > _id_table;
    Node* _nodes[eMAX_NODES];
    NodeInstanceInfo _instance_info[eMAX_NODES];

    bxRecursiveBenaphore _lock_nodes;

    void startup()
    {
        memset( _nodes, 0x00, sizeof( _nodes ) );
        for( int i = 0; i < eMAX_NODES; ++i )
        {
            nodeInstanceInfoClear( &_instance_info[i] );
        }
    }

    void shutdown()
    {
        SYS_ASSERT( id_table::size( _id_table ) == 0 );
        for( int i = 0; i < eMAX_NODES; ++i )
        {
            SYS_ASSERT( _nodes[i] == nullptr );
            SYS_ASSERT( nodeInstanceInfoEmpty( _instance_info[i] ) );
        }

        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            if( _node_types[i].info._type_deinit )
            {
                ( *_node_types[i].info._type_deinit )( );
            }
        }
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
        type.id = index;
        type.info = info;
        type.info._type_name = string::duplicate( nullptr, info._type_name );

        if( info._type_init )
        {
            ( *info._type_init )( );
        }

        return index;
    }

    id_t nodeCreate( const char* typeName, const char* nodeName )
    {

        int typeIndex = typeFind( typeName );
        if( typeIndex == -1 )
        {
            bxLogError( "Node type '%s' not found", typeName );
            return makeInvalidHandle<id_t>();
        }

        NodeType* type = &_node_types[typeIndex];
        Node* node = ( *type->info._creator )( );
        SYS_ASSERT( node != nullptr );

        _lock_nodes.lock();
        id_t id = id_table::create( _id_table );
        _lock_nodes.unlock();

        SYS_ASSERT( _nodes[id.index] == nullptr );
        _nodes[id.index] = node;

        NodeInstanceInfo& instance = _instance_info[id.index];
        SYS_ASSERT( nodeInstanceInfoEmpty( instance ) );

        instance._type_id = type->id;
        instance._instance_id = id;
        instance._type_name = type->info._type_name;
        instance._instance_name = string::duplicate( nullptr, nodeName );

        return id;
    }

};
static GraphGlobal* __graphGlobal = nullptr;
void graphGlobalStartup()
{
    SYS_ASSERT( __graphGlobal == nullptr );
    __graphGlobal = BX_NEW( bxDefaultAllocator(), GraphGlobal );
    __graphGlobal->startup();
}
void graphGlobalShutdown()
{
    if( !__graphGlobal )
        return;

    __graphGlobal->shutdown();
    BX_DELETE0( bxDefaultAllocator(), __graphGlobal );
}

bool nodeRegister( const NodeTypeInfo& typeInfo )
{
    int ires = __graphGlobal->typeAdd( typeInfo );
    return ( ires == -1 ) ? false : true;
}

struct Graph
{};




}////