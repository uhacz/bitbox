#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_backend.h>


class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();

        bxGdi::backendStartup( &_gdiDev, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        return true;
    }
    virtual void shutdown()
    {
        bxGdi::backendShutdown( &_gdiDev );
    }
    virtual bool update()
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isPeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        return true;
    }

    bxGdiDeviceBackend* _gdiDev;
};

int main( int argc, const char* argv[] )
{
    bxWindow* window = bxWindow_create( "demo", 1280, 720, false, 0 );
    if( window )
    {
        bxDemoApp app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }
    
    
    return 0;
}