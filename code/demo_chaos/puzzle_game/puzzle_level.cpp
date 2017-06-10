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

    physics::Create( &_solver, 1024 * 8, 0.1f );
    physics::SetFrequency( _solver, 120 );

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
        physics::SetBodyParams( _solver, _rope[i], params );


    }
}

void LevelState::OnShutDown()
{
    for( u32 i = 0; i < NUM_ROPES; i++ )
        physics::DestroyBody( _solver, _rope[i] );

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

    for( size_t i = 0; i < NUM_ROPES; i++ )
    {
        physics::DebugDraw( _solver, _rope[i], physics::DebugDrawBodyParams().NoConstraints() );
    }

    {// test box
        u32 w = 4;
        u32 h = 4;
        u32 d = 4;

        const f32 pradius = physics::GetParticleRadius( _solver );
        const f32 pradius2 = pradius * 2.f;

        Matrix4F pose = Matrix4F::translation( Vector3F( 5.f, 5.f, 0.f ) );

        const Vector3F begin_pos = Vector3F( (f32)w, (f32)h, (f32)d ) * -0.5f * pradius2;
        
        for( u32 iz = 0; iz < d; ++iz )
        {
            for( u32 iy = 0; iy < h; ++iy )
            {
                for( u32 ix = 0; ix < w; ++ix )
                {
                    const Vector3F relative_pos = Vector3F( (f32)ix, (f32)iy, (f32)iz ) * pradius2;
                    const Vector3F pos = ( pose * Point3F( begin_pos + relative_pos ) ).getXYZ();

                    rdi::debug_draw::AddSphere( Vector4F( pos, pradius ), 0xFF0000FF, 1 );

                }
            }
        }

    }

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