#include "puzzle_level.h"

#include "..\game_util.h"
#include "rdi\rdi_debug_draw.h"
#include "util\common.h"
#include "..\..\system\window.h"

namespace bx { namespace puzzle {

void PuzzleGame::StartUpImpl()
{
    GameSimple::StartUpImpl();

    LevelState* lstate = BX_NEW( bxDefaultAllocator(), LevelState, &_gfx );
    GameStateId level_state_id = AddState( lstate );
    PushState( level_state_id );

}

void LevelState::OnStartUp()
{
    _gfx_scene = _gfx->renderer.CreateScene( "terrainLevel" );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );

    game_util::CreateDebugMaterials();

    gfx::Camera& camera = GetGame()->GetDevCamera();
    camera.world = Matrix4( Matrix3::rotationX( -PI / 4 ), Vector3( 0.f, 20.f, 21.f ) );

    const float width = 64.f;
    const float depth = 64.f;
    {
        gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "grey" ) );
        _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":box" ) );

        Matrix4 pose[] =
        {
            appendScale( Matrix4::translation( Vector3( 0.f, 0.f, 0.f ) ), Vector3( width, 0.1f, depth ) ),
        };
        _gfx_scene->SetMatrices( actor, pose, 1 );
    }

    _player = PlayerCreate( "playerLocal" );

}

void LevelState::OnShutDown()
{
    PlayerDestroy( _player );

    _gfx->renderer.DestroyScene( &_gfx_scene );
}

void LevelState::OnUpdate( const GameTime& time )
{
    const gfx::Camera& camera = GetGame()->GetDevCamera();

    bxWindow* window = bxWindow_get();
    const bxInput& input = window->input;

    const Matrix3F player_basis = toMatrix3F( camera.world.getUpper3x3() );
    PlayerCollectInput( _player, input, player_basis, time.delta_time_us );


    PlayerTick( time.delta_time_us );
}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    active_camera = &GetGame()->GetDevCamera();

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    PlayerDraw( _player );

    gfx::Scene gfx_scene = _gfx_scene;

    //// ---
    _gfx->DrawScene( cmdq, gfx_scene, *active_camera );
    _gfx->PostProcess( cmdq, *active_camera, time.DeltaTimeSec() );
    _gfx->Rasterize( cmdq, *active_camera );
}

}}//