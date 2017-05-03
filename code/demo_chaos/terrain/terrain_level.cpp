#include "terrain_level.h"
#include "..\game_util.h"
#include "rdi\rdi_debug_draw.h"
#include "util\common.h"

namespace bx { namespace terrain {

void TerrainGame::StartUpImpl()
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

    terrain::CreateInfo create_info = {};
    create_info.tile_side_length = 5.f;
    create_info.radius[0] = 1.f;
    create_info.radius[1] = 2.f;
    create_info.radius[2] = 3.f;
    create_info.radius[3] = 4.f;
    _tinstance = terrain::Create( create_info );

    gfx::Camera& camera = GetGame()->GetDevCamera();
    camera.world = Matrix4( Matrix3::rotationX( -PI/2 ), Vector3( 0.f, 35.f, 10.f ) );

}

void LevelState::OnShutDown()
{
    terrain::Destroy( &_tinstance );
    _gfx->renderer.DestroyScene( &_gfx_scene );
}

void LevelState::OnUpdate( const GameTime& time )
{
    const gfx::Camera& camera = GetGame()->GetDevCamera();
    terrain::Tick( _tinstance, toMatrix4F( camera.world ) );
}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    active_camera = &GetGame()->GetDevCamera();

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Scene gfx_scene = _gfx_scene;

    //// ---
    _gfx->DrawScene( cmdq, gfx_scene, *active_camera );
    _gfx->PostProcess( cmdq, *active_camera, time.DeltaTimeSec() );
    _gfx->Rasterize( cmdq, *active_camera );
}

}
}//