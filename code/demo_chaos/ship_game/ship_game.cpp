#include "ship_game.h"
#include "..\game_gui.h"
#include "..\game_util.h"

#include <util\memory.h>
#include <util\string_util.h>
#include <system\window.h>
#include <resource_manager\resource_manager.h>
#include <rdi\rdi_debug_draw.h>

#include "ship_level.h"
#include "..\imgui\imgui.h"
#include "..\imgui\imgui_impl_dx11.h"

namespace bx{
namespace ship{


ShipGame::ShipGame(){}
ShipGame::~ShipGame(){}

void ShipGame::StartUpImpl()
{
    game_gui::StartUp();
    game_gfx::StartUp( &_gfx );

    game_util::CreateDebugMaterials();

    LevelState* level_state = BX_NEW( bxDefaultAllocator(), LevelState, &_gfx );
    GameStateId level_state_id = AddState( level_state );
    PushState( level_state_id );
}


void ShipGame::ShutDownImpl()
{
    game_gfx::ShutDown( &_gfx );
    game_gui::ShutDown();
}

bool ShipGame::PreUpdateImpl( const GameTime& time )
{
    game_gui::NewFrame();
    return true;
}

void ShipGame::PreRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    _gfx.renderer.BeginFrame( cmdq );
}

void ShipGame::PostRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    game_gui::Render();
    _gfx.renderer.EndFrame( cmdq );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LevelState::OnStartUp()
{
    _dev_camera.params.zFar = 1000.f;
    
    _level = BX_NEW( bxDefaultAllocator(), Level );
    _level->StartUp( _gfx, "level" );
}

void LevelState::OnShutDown()
{
    if( _level )
    {
        _level->ShutDown( _gfx );
        BX_DELETE0( bxDefaultAllocator(), _level );
    }
}

void LevelState::OnUpdate( const GameTime& time )
{
    bxWindow* win = bxWindow_get();
    if( bxInput_isKeyPressedOnce( &win->input.kbd, '1' ) )
    {
        _use_dev_camera = !_use_dev_camera;
    }
    if( _use_dev_camera )
    {
        game_util::DevCameraCollectInput( &_dev_camera_input_ctx, time.DeltaTimeSec(), 0.01f );
        _dev_camera.world = _dev_camera_input_ctx.computeMovement( _dev_camera.world, 0.15f );

        gfx::computeMatrices( &_dev_camera );
    }
    
    if( _level )
    {
        _level->Tick( time );
    }
}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    if( _use_dev_camera )
    {
        active_camera = &_dev_camera;
    }
       
    if( !_level )
        return;
    
    if( !active_camera )
    {
        active_camera = &_level->_player_camera._camera;
    }

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Scene gfx_scene = _level->_gfx_scene;

    // ---
    _gfx->DrawScene( cmdq, gfx_scene, *active_camera );
    _gfx->PostProcess( cmdq, *active_camera, time.DeltaTimeSec() );
    _gfx->Rasterize( cmdq, *active_camera );
}

}
}//

