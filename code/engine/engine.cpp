#include "engine.h"
#include <util/config.h>

#include <system/window.h>

#include <gfx/gfx_gui.h>
#include <gfx/gfx_debug_draw.h>

void bxEngine_startup( bxEngine* e )
{
    bxConfig::global_init();

    bxWindow* win = bxWindow_get();
    const char* assetDir = bxConfig::global_string( "assetDir" );
    e->resourceManager = bxResourceManager::startup( assetDir );
    bxGdi::backendStartup( &e->gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

    e->gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
    e->gdiContext->_Startup( e->gdiDevice );

    e->materialManager = BX_NEW( bxDefaultAllocator(), bxGfxMaterialManager );
    e->materialManager->_Startup( e->gdiDevice, e->resourceManager, "native1" );

    bxGfx::globalResources_startup( e->gdiDevice, e->resourceManager );

    bxGfxGUI::_Startup( e->gdiDevice, e->resourceManager, win );
    bxGfxDebugDraw::_Startup( e->gdiDevice, e->resourceManager );

    rmt_CreateGlobalInstance( &e->_remotery );
}
void bxEngine_shutdown( bxEngine* e )
{
    rmt_DestroyGlobalInstance( e->_remotery );
    e->_remotery = 0;

    bxGfxDebugDraw::_Shutdown( e->gdiDevice, e->resourceManager );
    bxGfxGUI::shutdown( e->gdiDevice, e->resourceManager, bxWindow_get() );

    bxGfx::globalResources_shutdown( e->gdiDevice, e->resourceManager );

    e->materialManager->_Shutdown( e->gdiDevice, e->resourceManager );
    BX_DELETE0( bxDefaultAllocator(), e->materialManager );

    e->gdiContext->_Shutdown();
    BX_DELETE0( bxDefaultAllocator(), e->gdiContext );

    bxGdi::backendShutdown( &e->gdiDevice );
    bxResourceManager::shutdown( &e->resourceManager );
    
    bxConfig::global_deinit();
}
