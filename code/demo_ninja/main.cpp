#include <system/application.h>
#include <system/window.h>
#include <util/time.h>
#include <util/config.h>

#include "gfx.h"

class App : public bxApplication
{
public:
    App()
        : _gfx( nullptr )
        , _renderCtx( nullptr )
        , _resourceManager( nullptr )
        , _timeMS(0)
    {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bxConfig::global_init();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        _resourceManager = bxResourceManager::startup( assetDir );

        bx::gfxStartup( &_gfx, win->hwnd, true, false );
        bx::gfxLinesContextCreate( &_renderCtx, _gfx, _resourceManager );
        return true;
    }
    virtual void shutdown()
    {
        bx::gfxLinesContextDestroy( &_renderCtx, _gfx );
        bx::gfxShutdown( &_gfx );
        bxResourceManager::shutdown( &_resourceManager );

        bxConfig::global_deinit();
    }

    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;

        _timeMS += deltaTimeUS / 1000;

        return true;
    }

    bx::GfxContext* _gfx;
    bx::GfxLinesContext* _renderCtx;
    bxResourceManager* _resourceManager;

    u64 _timeMS;
};

int main( int argc, const char** argv )
{
    bxWindow* window = bxWindow_create( "ninja", 1280, 720, false, 0 );
    if( window )
    {
        App app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }

    return 0;
}
