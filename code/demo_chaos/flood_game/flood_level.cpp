#include "flood_level.h"

#include "../renderer.h"
#include "../game_gfx.h"
#include "../game_util.h"

#include <util/string_util.h>
#include <rdi/rdi_debug_draw.h>

namespace bx{ namespace flood{


//////////////////////////////////////////////////////////////////////////


void Level::StartUp( game_gfx::Deffered* gfx, const char* levelName )
{
    _name = string::duplicate( (char*)_name, levelName );
    _gfx_scene = gfx->renderer.CreateScene( levelName );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );

    game_util::CreateDebugMaterials();

    const float width  = _volume_width  * _world_scale; //10.24f;
    const float height = _volume_height * _world_scale; //2.56f;
    const float depth  = _volume_depth  * _world_scale; //5.12f;

    {
        gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "grey" ) );
        _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":box" ) );
        
        Matrix4 pose[] =
        {
            appendScale( Matrix4::translation( Vector3( 0.f, -height, 0.f ) ), Vector3( width, 0.1f, depth ) ),
        };
        _gfx_scene->SetMatrices( actor, pose, 1 );
    }

    // --- border planes
    {
        _plane_right  = makePlane( -Vector3::xAxis(), Vector3( width, 0.f, 0.f ) );
        _plane_bottom = makePlane(  Vector3::yAxis(), Vector3( 0.f, -height, 0.f ) );
        _plane_left   = makePlane(  Vector3::xAxis(), Vector3( -width, 0.f, 0.f ) );
        _plane_top    = makePlane( -Vector3::yAxis(), Vector3( 0.f, height * 0.5f, 0.f ) );
        _plane_front  = makePlane(  Vector3::zAxis(), Vector3( 0.f, 0.f, -depth * 0.5f ) );
        _plane_back   = makePlane( -Vector3::zAxis(), Vector3( 0.f, 0.f, depth * 0.5f ) );
    }


    const float particle_radius = 0.1f;
    FluidCreate( &_fluid, 7*7*7, particle_radius );

    {
        const u32 num_particles[3] = 
        {
            _volume_width * particle_radius,
            _volume_height * particle_radius,
            _volume_depth * particle_radius,
        };
        StaticBodyCreateBox( &_boundary[0], 1, num_particles[1], num_particles[2], particle_radius, Matrix4F::translation( Vector3F( width+particle_radius, 0.f, 0.f ) ) );
        StaticBodyCreateBox( &_boundary[1], num_particles[0], 1, num_particles[2], particle_radius, Matrix4F::translation( Vector3F( 0.0f,-height + particle_radius, 0.f ) ) );
        StaticBodyCreateBox( &_boundary[2], 1, num_particles[1], num_particles[2], particle_radius, Matrix4F::translation( Vector3F( -width + particle_radius, 0.f, 0.f ) ) );
        
        StaticBodyDoNeighbourMap( &_boundary[0], particle_radius * 4 );
        StaticBodyDoNeighbourMap( &_boundary[1], particle_radius * 4 );
        StaticBodyDoNeighbourMap( &_boundary[2], particle_radius * 4 );


    }

    //const Matrix4 init_pose = Matrix4( Matrix3::rotationZ( PI / 4 ), Vector3( 0.f ) );
    const Matrix4F init_pose = Matrix4F::identity();
    FluidInitBox( &_fluid, init_pose );

}

void Level::ShutDown( game_gfx::Deffered* gfx )
{
    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;
}

void Level::Tick( const GameTime& time )
{
    FluidColliders colliders;
    colliders.planes = &_plane_right;
    colliders.num_planes = 6;
    colliders.static_bodies = _boundary;
    colliders.num_static_bodies = 3;

    FluidSimulationParams sim_params = {};
    //sim_params.gravity = Vector3( 0.f, -0.1f, 0.f ); // Vector3::yAxis();

    FluidTick( &_fluid, sim_params, colliders, time.DeltaTimeSec() );

    StaticBodyDebugDraw( _boundary[0], 0x333333FF );
    StaticBodyDebugDraw( _boundary[1], 0x333333FF );
    StaticBodyDebugDraw( _boundary[2], 0x333333FF );


}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}


}
}//
