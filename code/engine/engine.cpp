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

    bxGfxGUI::_Startup( e->gdiDevice, win );
    bxGfxDebugDraw::_Startup( e->gdiDevice );

    bx::gfxContextStartup( &e->gfxContext, e->gdiDevice );
    bx::phxContextStartup( &e->phxContext, 4 );

    rmt_CreateGlobalInstance( &e->_remotery );
}
void bxEngine_shutdown( bxEngine* e )
{
    rmt_DestroyGlobalInstance( e->_remotery );
    e->_remotery = 0;

    bx::phxContextShutdown( &e->phxContext );
    bx::gfxContextShutdown( &e->gfxContext, e->gdiDevice );

    bxGfxDebugDraw::_Shutdown( e->gdiDevice );
    bxGfxGUI::_Shutdown( e->gdiDevice, bxWindow_get() );

    e->gdiContext->_Shutdown();
    BX_DELETE0( bxDefaultAllocator(), e->gdiContext );

    bxGdi::backendShutdown( &e->gdiDevice );
    bxResourceManager::shutdown( &e->resourceManager );
    
    bxConfig::global_deinit();
}
