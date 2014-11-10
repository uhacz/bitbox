#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_shader.h>
#include <gdi/gdi_render_source.h>
#include <resource_manager/resource_manager.h>


static bxGdiShaderFx* fx = 0;
static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        _resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        fx = bxGdi::shaderFx_createFromFile( _gdiDevice, _resourceManager, "test" );
        fxI = bxGdi::shaderFx_createInstance( _gdiDevice, fx );
        rsource = bxGdi::renderSource_new( 3 );

        return true;
    }
    virtual void shutdown()
    {
        bxGdi::renderSource_releaseAndFree( _gdiDevice, &rsource );
        bxGdi::shaderFx_releaseInstance( _gdiDevice, &fxI );
        bxGdi::shaderFx_release( _gdiDevice, &fx );

        bxGdi::backendShutdown( &_gdiDevice );
        bxResourceManager::shutdown( &_resourceManager );
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
        gdiContext->clearBuffers( 0, 0, bxGdiTexture(), clearColor, 1, 1 );


        gdiContext->swap();

        return true;
    }

    bxGdiDeviceBackend* _gdiDevice;
    bxResourceManager* _resourceManager;
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