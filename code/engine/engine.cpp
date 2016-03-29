#include "engine.h"
#include <util/config.h>

#include <system/window.h>

#include <gfx/gfx_gui.h>
#include <gfx/gfx_debug_draw.h>
#include <system/input.h>

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
struct GraphGlobal
{
    
};

struct Graph
{};




}////