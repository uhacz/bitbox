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

    {
        gfx::ActorID actor = _gfx_scene->Add( "sphere", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "blue" ) );
        _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":sphere" ) );

        Matrix4 pose[] =
        {
            Matrix4::translation( Vector3( 6.f, 2.f, 0.f ) ),
        };
        _gfx_scene->SetMatrices( actor, pose, 1 );
    }

    _player = PlayerCreate( "playerLocal" );

    physics::Create( &_solver, 1024 * 8, 0.2f );
    physics::SetFrequency( _solver, 60 );
    physics::Create( &_solver_gfx, _solver, _gfx_scene );

    const Vector3F axis[5] =
    {
        Vector3F::xAxis(),
        Vector3F::zAxis(),
        normalize( -Vector3F::yAxis() + Vector3F::zAxis() ),
        -Vector3F::zAxis(),
        -Vector3F::xAxis(),
    };

    for( u32 i = 0; i < NUM_ROPES; i++ )
    {
        _rope[i] = physics::CreateRope( _solver, Vector3F( -( i / 2.f ) * 2.f, 13.f, 0.f ), axis[i % 5], 15.f, 1.f + ( i * 0.5f ) );
        float* mass_inv = physics::MapMassInv( _solver, _rope[i] );
        mass_inv[0] = 0.f;
        physics::Unmap( _solver, mass_inv );

        physics::BodyParams params;
        physics::GetBodyParams( &params, _solver, _rope[i] );
        params.restitution = 1.0f;
        params.dynamic_friction = 0.1f;
        physics::SetBodyParams( _solver, _rope[i], params );

        physics::AddBody( _solver_gfx, _rope[i] );
    }


    const float a = 2.f;
    Matrix4F soft_pose0 = Matrix4F( Matrix3F::rotationZYX( Vector3F(PI/4, PI/4,PI/4) ), Vector3F( 0.f, 13.f, 2.f ) );
    _soft0 = physics::CreateSoftBox( _solver, soft_pose0, a,a,a, 1.f );

    Matrix4F soft_pose1 = Matrix4F( Matrix3F::rotationZYX( Vector3F( 0.f ) ), Vector3F( 2.f, 3.f, 2.f ) );
    _soft1 = physics::CreateSoftBox( _solver, soft_pose1, a,a,a, 3.f );

    physics::AddBody( _solver_gfx, _soft0 );
    physics::AddBody( _solver_gfx, _soft1 );
}

void LevelState::OnShutDown()
{
    for( u32 i = 0; i < NUM_ROPES; i++ )
        physics::DestroyBody( _solver, _rope[i] );

    physics::Destroy( &_solver_gfx );
    physics::Destroy( &_solver );
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
    physics::Solve( _solver, 4, time.DeltaTimeSec() );

    //for( size_t i = 0; i < NUM_ROPES; i++ )
    //{
    //    physics::DebugDraw( _solver, _rope[i], physics::DebugDrawBodyParams().NoConstraints() );
    //}

    //physics::DebugDraw( _solver, _soft0, physics::DebugDrawBodyParams().Points( 0xFF0000FF ) );
    //physics::DebugDraw( _solver, _soft1, physics::DebugDrawBodyParams().Points( 0x00FF00FF ) );

}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    active_camera = &GetGame()->GetDevCamera();

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    PlayerDraw( _player );

    gfx::Scene gfx_scene = _gfx_scene;

    _gfx->PrepareScene( cmdq, gfx_scene, *active_camera );
    
    const gfx::ShadowPass::LightMatrices& lightMatrices = _gfx->shadow_pass.GetMatrices();
    physics::Tick( _solver_gfx, cmdq, *active_camera, lightMatrices.world, lightMatrices.proj );

    //// ---
    _gfx->Draw( cmdq );
    _gfx->PostProcess( cmdq, *active_camera, time.DeltaTimeSec() );
    _gfx->Rasterize( cmdq, *active_camera );
}

}}//