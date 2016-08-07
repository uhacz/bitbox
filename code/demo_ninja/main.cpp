#include <system/application.h>
#include <system/window.h>
#include <util/time.h>
#include <util/config.h>
#include <util/signal_filter.h>

#include <engine/engine.h>
#include <gdi/gdi.h>
#include "renderer.h"

typedef bx::gdi::CommandBucket<u64> CommandBucket64;
CommandBucket64* gColorBucket = nullptr;

using namespace bx;
class App : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bx::rendererStartup( win );
        
        //gColorBucket = CommandBucket64::create( _engine.gdi_device, 1024*8, 1024*1024 );



        //const char* assetDir = bxConfig::global_string( "assetDir" );
        return true;
    }
    virtual void shutdown()
    {
        //CommandBucket64::destroy( _engine.gdi_device, &gColorBucket );
        bxWindow* win = bxWindow_get();

        bx::rendererShutdown();

        //bx::Engine::shutdown( &_engine );
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
                
        _time_us += deltaTimeUS;

        return true;
    }

    //bx::Engine _engine;
    u64 _time_us{0};
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
