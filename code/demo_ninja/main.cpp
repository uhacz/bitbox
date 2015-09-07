#include <system/application.h>
#include <system/window.h>
#include <util/time.h>

#include "gfx.h"


class App : public bxApplication
{
public:
    App()
        : _gfx(0)
        , _timeMS(0)
    {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bx::gfxStartup( &_gfx, win->hwnd, true, false );
        return true;
    }
    virtual void shutdown()
    {
        bx::gfxShutdown( &_gfx );
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
