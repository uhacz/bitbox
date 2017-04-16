#include "flood_game.h"
#include "..\game_gui.h"
#include "..\game_util.h"

#include <system\input.h>
#include <system\window.h>
#include <rdi\rdi_debug_draw.h>

#include "flood_level.h"
#include "util\common.h"


namespace bx {namespace flood {

void FloodGame::StartUpImpl()
{
    GameSimple::StartUpImpl();
    LevelState* level_state = BX_NEW( bxDefaultAllocator(), LevelState, &_gfx );
    GameStateId level_state_id = AddState( level_state );
    PushState( level_state_id );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


void LevelState::OnStartUp()
{
    _level = BX_NEW( bxDefaultAllocator(), Level );
    _level->StartUp( _gfx, "level" );

    GetGame()->GetDevCamera().world = Matrix4( Matrix3::rotationX( -PI/8 ),  Vector3( 0.f, 4.5f, 9.f ) );
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
    if( _level )
    {
        _level->Tick( time );
    }
}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    active_camera = &GetGame()->GetDevCamera();

    if( !_level )
        return;

    //if( !active_camera )
    //{
    //    active_camera = &_level->_player_camera._camera;
    //}

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Scene gfx_scene = _level->_gfx_scene;

    //// ---
    _gfx->DrawScene( cmdq, gfx_scene, *active_camera );
    _gfx->PostProcess( cmdq, *active_camera, time.DeltaTimeSec() );
    _gfx->Rasterize( cmdq, *active_camera );
}

}}///
