#include "no_gods_no_masters.h"
#include <system\window.h>
#include <util/config.h>

namespace tjdb
{
    
    bool NoGodsNoMasters::startup( int argc, const char** argv )
    {
        const char* cfg_filename = "demo_tjdb/global.cfg";
        bxConfig::global_init( cfg_filename );

        bxWindow* win = bxWindow_get();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        ResourceManager::startup( assetDir );
        
        rdi::Startup( (uptr)win->hwnd, win->width, win->height, win->full_screen );
        
        // ---
        _fullscreen_quad = rdi::CreateFullscreenQuad();

        return true;
    }

    void NoGodsNoMasters::shutdown()
    {
        rdi::DestroyRenderSource( &_fullscreen_quad );

        // ---
        rdi::Shutdown();

        ResourceManager* rsMan = GResourceManager();
        ResourceManager::shutdown( &rsMan );
        bxConfig::global_deinit();
    }

    bool NoGodsNoMasters::update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const u64 deltaTimeMS = deltaTimeUS / 1000;

        return true;
    }

}///

