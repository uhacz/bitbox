#include <system/application.h>
#include <system/window.h>

#include <util/config.h>
#include <resource_manager/resource_manager.h>

#include "test_game/test_game.h"
#include "ship_game/ship_game.h"
#include "flood_game/flood_game.h"
#include "puzzle_game/puzzle_level.h"
#include "terrain/terrain_level.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
using namespace bx;

 //void bx_purecall_handler()
 //{
 //    SYS_ASSERT( false );
 //}

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxConfig::global_init( "demo_chaos/global.cfg" );
        const char* assetDir = bxConfig::global_string( "assetDir" );
        bx::ResourceManager::startup( assetDir );
        
        bxWindow* win = bxWindow_get();
        rdi::Startup( (uptr)win->hwnd, win->width, win->height, win->full_screen );
        
        //bxAsciiScript sceneScript;
        //if( _engine._camera_script_callback )
        //    _engine._camera_script_callback->addCallback( &sceneScript );

        //const char* sceneName = bxConfig::global_string( "scene" );
        //bxFS::File scriptFile = _engine.resource_manager->readTextFileSync( sceneName );

        //if( scriptFile.ok() )
        //{
        //    bxScene::script_run( &sceneScript, scriptFile.txt );
        //}
        //scriptFile.release();

        //{
        //    char const* cameraName = bxConfig::global_string( "camera" );
        //    if( cameraName )
        //    {
        //        bx::GfxCamera* camera = _engine.camera_manager->find( cameraName );
        //        if( camera )
        //        {
        //            _engine.camera_manager->stack()->push( camera );
        //            _camera = camera;
        //        }
        //    }
        //}

        //_game = BX_NEW( bxDefaultAllocator(), bx::ship::ShipGame );
        //_game = BX_NEW( bxDefaultAllocator(), bx::flood::FloodGame );
        //_game = BX_NEW( bxDefaultAllocator(), bx::terrain::TerrainGame );
        _game = BX_NEW( bxDefaultAllocator(), bx::puzzle::PuzzleGame );
        _game->StartUp();

        return true;
    }
    virtual void shutdown()
    {
        _game->ShutDown();
        BX_DELETE0( bxDefaultAllocator(), _game );

        rdi::Shutdown();
        ResourceManager::shutdown();
        bxConfig::global_deinit();
    }

    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        rmt_ScopedCPUSample( MainLoop, 0 );

        bool is_game_running = _game->Update();
        if( is_game_running )
        {
            _game->Render();
        }
        return is_game_running;
    }
    bx::Game* _game = nullptr;
};

//////////////////////////////////////////////////////////////////////////
int main( int argc, const char* argv[] )
{
    bx::memory::StartUp();
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

    bx::memory::ShutDown();
    return 0;
}