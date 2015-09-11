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
        , _renderData( nullptr )
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
        bx::gfxLinesDataCreate( &_renderData, _gfx, 1024 * 8 );

        _camera.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );

        return true;
    }
    virtual void shutdown()
    {
        bx::gfxLinesDataDestroy( &_renderData, _gfx );
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

        bx::GfxCommandQueue* cmdQueue = bx::gfxAcquireCommandQueue( _gfx );

        const Vector3 positions[] =
        {
            Vector3( -0.5f, -0.5f, 0.f ), Vector3( 0.5f, 0.5f, 0.f ),
        };
        const Vector3 normals[] =
        {
            Vector3::zAxis(), Vector3::yAxis(),
        };
        const u32 colors[] = 
        {
            0xFFFFFFFF, 0xFF00FF00,
        };

        bx::gfxLinesDataAdd( _renderData, 2, positions, normals, colors );
        bx::gfxLinesDataUpload( cmdQueue, _renderData );
                
        bx::gfxFrameBegin( _gfx, cmdQueue );

        bx::gfxCameraComputeMatrices( &_camera );
        
        bx::gfxViewportSet( cmdQueue, _camera );


        bx::gfxLinesDataFlush( cmdQueue, _renderCtx, _renderData );
        bx::gfxLinesDataClear( _renderData );

        bx::gfxReleaseCommandQueue( &cmdQueue );
        bx::gfxFrameEnd( _gfx );

        _timeMS += deltaTimeUS / 1000;

        

        return true;
    }

    bx::GfxCamera _camera;
    bx::GfxContext* _gfx;
    bx::GfxLinesContext* _renderCtx;
    bx::GfxLinesData* _renderData;
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
