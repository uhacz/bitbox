#include "flood_level.h"

#include "../renderer.h"
#include "../game_gfx.h"
#include "../game_util.h"

#include <util/string_util.h>
#include <rdi/rdi_debug_draw.h>
#include "util/grid.h"
#include "util/common.h"

namespace bx{ namespace flood{

//////////////////////////////////////////////////////////////////////////
void FluidClear( Fluid* f )
{
    array::clear( f->x );
    array::clear( f->p );
    array::clear( f->v );
}

void FluidCreate( Fluid* f, u32 numParticles, float particleRadius )
{
    FluidClear( f );
    array::reserve( f->x, numParticles );
    array::reserve( f->p, numParticles );
    array::reserve( f->v, numParticles );
    f->_particle_radius = particleRadius;

    for( u32 i = 0; i < numParticles; ++i )
    {
        array::push_back( f->x, Vector3( 0.f ) );
        array::push_back( f->p, Vector3( 0.f ) );
        array::push_back( f->v, Vector3( 0.f ) );
    }
}

void FluidInitBox( Fluid* f, const Matrix4& pose )
{
    float fa = ::pow( (float)f->NumParticles(), 1.f / 3.f );
    u32 a = (u32)fa;
    
    const float offset = -fa * 0.5f;
    const float spacing = f->_particle_radius * 1.1f;

    const bxGrid grid( a, a, a );


    for( u32 z = 0; z < a; ++z )
    {
        for( u32 y = 0; y < a; ++y )
        {
            for( u32 x = 0; x < a; ++x )
            {
                const float fx = ( offset + x ) * spacing;
                const float fy = ( offset + y ) * spacing;
                const float fz = ( offset + z ) * spacing;

                Vector3 pos( fx, fy, fz );
                pos = mulAsVec4( pose, pos );

                const u32 index = grid.index( x, y, z );
                SYS_ASSERT( index < f->NumParticles() );

                f->x[index] = pos;
                f->p[index] = pos;
                f->v[index] = Vector3(0.f);
            }
        }
    }
}

void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime )
{
    const u32 n = f->NumParticles();
    for( u32 i = 0; i < n; ++i )
    {
        Vector3 v = f->v[i] + params.gravity * deltaTime;
        Vector3 p = f->x[i] + v*deltaTime;

        f->v[i] = v;
        f->p[i] = p;
    }

    for( u32 i = 0; i < n; ++i )
    {
        Vector3 p = f->p[i];

        for( u32 j = 0; j < colliders.num_planes; ++j )
        {
            const Vector4& plane = colliders.planes[j];
            const floatInVec d = dot( plane, Vector4( p, oneVec ) );
            Vector3 dpos = -plane.getXYZ() * minf4( d, zeroVec );
            
            p += dpos;
        }

        f->p[i] = p;
    }

    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    for( u32 i = 0; i < n; ++i )
    {
        f->v[i] = ( f->p[i] - f->x[i] ) * delta_time_inv;
        f->x[i] = f->p[i];
    }
    
    for( Vector3 pos : f->x )
    {
        rdi::debug_draw::AddBox( Matrix4::translation( pos ), Vector3( f->_particle_radius ), 0x0000FFFF, 1 );
        //rdi::debug_draw::AddSphere( Vector4( pos, f->_particle_radius ), 0x0000FFFF, 1 );
    }



}
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
            appendScale( Matrix4::translation( Vector3( 0.f, -height * 0.5f, 0.f ) ), Vector3( width, 0.1f, depth ) ),
        };
        _gfx_scene->SetMatrices( actor, pose, 1 );
    }

    // --- border planes
    {
        _plane_right  = makePlane( -Vector3::xAxis(), Vector3( width * 0.5f, 0.f, 0.f ) );
        _plane_bottom = makePlane(  Vector3::yAxis(), Vector3( 0.f, -height * 0.5f, 0.f ) );
        _plane_left   = makePlane(  Vector3::xAxis(), Vector3( -width * 0.5f, 0.f, 0.f ) );
        _plane_top    = makePlane( -Vector3::yAxis(), Vector3( 0.f, height * 0.5f, 0.f ) );
        _plane_front  = makePlane(  Vector3::zAxis(), Vector3( 0.f, 0.f, -depth * 0.5f ) );
        _plane_back   = makePlane( -Vector3::zAxis(), Vector3( 0.f, 0.f, depth * 0.5f ) );
    }

    FluidCreate( &_fluid, 1024, 0.1f );

    //const Matrix4 init_pose = Matrix4( Matrix3::rotationZ( PI / 4 ), Vector3( 0.f ) );
    const Matrix4 init_pose = Matrix4::identity();
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

    FluidSimulationParams sim_params = {};
    //sim_params.gravity = Vector3::yAxis();

    FluidTick( &_fluid, sim_params, colliders, time.DeltaTimeSec() );
}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}


}
}//
