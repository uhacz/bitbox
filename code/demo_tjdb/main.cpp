#include <system/application.h>

#include "tjdb.h"

#include "system/window.h"

#include "util/config.h"
#include "util/time.h"

#include <gdi/gdi_backend.h>
#include "gdi/gdi_context.h"

#include "resource_manager/resource_manager.h"
#include "gfx/gfx_gui.h"

class App : public bxApplication
{
public:
    App()
        : _resourceManager( nullptr )
        , _gdiDev( nullptr )
        , _gdiCtx( nullptr )
    {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bxConfig::global_init();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        _resourceManager = bxResourceManager::startup( assetDir );

        bxGdi::backendStartup( &_gdiDev, (uptr)win->hwnd, win->width, win->height, win->full_screen );
        _gdiCtx = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiCtx->_Startup( _gdiDev );

        tjdb::startup( win, _gdiDev, _resourceManager );

        bxGfxGUI::_Startup( _gdiDev, _resourceManager, win );

        return true;
    }
    virtual void shutdown()
    {
        bxWindow* win = bxWindow_get();
        bxGfxGUI::shutdown( _gdiDev, _resourceManager, win );
        
        tjdb::shutdown( _gdiDev, _resourceManager );
        _gdiCtx->_Shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gdiCtx );
        bxGdi::backendShutdown( &_gdiDev );
        bxResourceManager::shutdown( &_resourceManager );
        bxConfig::global_deinit();
    }

    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if ( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const u64 deltaTimeMS = deltaTimeUS / 1000;
        
        bxGfxGUI::newFrame( deltaTimeMS * 0.001f );

        tjdb::tick( &win->input, _gdiCtx, deltaTimeMS );
        
        tjdb::draw( _gdiCtx );

        bxGfxGUI::draw( _gdiCtx );

        _gdiCtx->backend()->swap();

        return true;
    }

    bxResourceManager* _resourceManager;
    bxGdiDeviceBackend* _gdiDev;
    bxGdiContext* _gdiCtx;
};

int main( int argc, const char** argv )
{
    bxWindow* window = bxWindow_create( "tjdb", 1920, 1080, false, 0 );
    if ( window )
    {
        App app;
        if ( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }

    return 0;
}
