#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_backend.h>


class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();

        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );


        

        return true;
    }
    virtual void shutdown()
    {
        bxGdi::backendShutdown( &_gdiDevice );
    }
    virtual bool update()
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isPeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGdiContextBackend* gdiContext = _gdiDevice->ctx;

        float clearColor[5] = { 1.f, 0.f, 0.f, 1.f, 1.f };
        gdiContext->clearBuffers( 0, 0, bxGdi::nullTexture(), clearColor, 1, 1 );


        gdiContext->swap();

        return true;
    }

    bxGdiDeviceBackend* _gdiDevice;
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