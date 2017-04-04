#include "flood_level.h"

#include "../renderer.h"
#include "../game_gfx.h"
#include "../game_util.h"
#include "../imgui/imgui.h"

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
            appendScale( Matrix4::translation( Vector3( 0.f, -height, 0.f ) ), Vector3( width, 0.1f, depth )*2.f ),
        };
        _gfx_scene->SetMatrices( actor, pose, 1 );
    }

    // --- border planes
    {
        _plane_right  = makePlane( -Vector3F::xAxis(), Vector3F( width, 0.f, 0.f ) );
        _plane_bottom = makePlane(  Vector3F::yAxis(), Vector3F( 0.f, -height, 0.f ) );
        _plane_left   = makePlane(  Vector3F::xAxis(), Vector3F( -width, 0.f, 0.f ) );
        _plane_front  = makePlane(  Vector3F::zAxis(), Vector3F( 0.f, 0.f, -depth ) );
        _plane_back   = makePlane( -Vector3F::zAxis(), Vector3F( 0.f, 0.f, depth ) );
    }


    const float particle_radius = 0.05f;
    const Matrix4F init_pose = Matrix4F::translation( Vector3F( 0.5f, 3.f, 0.f ) );
    FluidCreateBox( &_fluid, 10, 20, 10, particle_radius, init_pose );

    {
        const float boundary_particle_radius = particle_radius;
        const u32 num_particles[3] = 
        {
            (u32)( width  / ( boundary_particle_radius ) ) + 1,
            (u32)( height / ( boundary_particle_radius ) ) + 1,
            (u32)( depth  / ( boundary_particle_radius ) ) + 1,
        };

        const float offset = boundary_particle_radius * 2.f;
        const float offset_half = boundary_particle_radius;

        StaticBodyCreateBox( &_boundary[0], 1, num_particles[1], num_particles[2], boundary_particle_radius, Matrix4F::translation( Vector3F( width + offset, 0.f, offset_half ) ) );
        StaticBodyCreateBox( &_boundary[1], num_particles[0], 1, num_particles[2], boundary_particle_radius, Matrix4F::translation( Vector3F( offset_half,-height, offset_half ) ) );
        StaticBodyCreateBox( &_boundary[2], 1, num_particles[1], num_particles[2], boundary_particle_radius, Matrix4F::translation( Vector3F(-width, 0.f, offset_half ) ) );
        StaticBodyCreateBox( &_boundary[3], num_particles[0], num_particles[1], 1, boundary_particle_radius, Matrix4F::translation( Vector3F( offset_half, 0.f,-depth ) ) );
        StaticBodyCreateBox( &_boundary[4], num_particles[0], num_particles[1], 1, boundary_particle_radius, Matrix4F::translation( Vector3F( offset_half, 0.f, depth + offset ) ) );

        const float neighbour_radius = particle_radius * 2.0f;
        StaticBodyDoNeighbourMap( &_boundary[0], neighbour_radius );
        StaticBodyDoNeighbourMap( &_boundary[1], neighbour_radius );
        StaticBodyDoNeighbourMap( &_boundary[2], neighbour_radius );
        StaticBodyDoNeighbourMap( &_boundary[3], neighbour_radius );
        StaticBodyDoNeighbourMap( &_boundary[4], neighbour_radius );

        for( u32 i = 0; i < 5; ++i )
        {
            StaticBodyComputeBoundaryPsi( &_boundary[i], _fluid.density0 );
        }
    }

    _fluid_sim_params.gravity = Vector3F( 0.f );

    _pbd_scene = BX_NEW( bxDefaultAllocator(), PBDScene, particle_radius );
    _pbd_cloth_solver = BX_NEW( bxDefaultAllocator(), PBDClothSolver, _pbd_scene );

    
    {
        const Vector3F cloth_points[16] =
        {
            Vector3F( 0.f, 0.0f, 0.f ), Vector3F( 0.1f, 0.0f, 0.f ), Vector3F( 0.2f, 0.0f, 0.f ), Vector3F( 0.3f, 0.0f, 0.f ),
            Vector3F( 0.f, 0.1f, 0.f ), Vector3F( 0.1f, 0.1f, 0.f ), Vector3F( 0.2f, 0.1f, 0.f ), Vector3F( 0.3f, 0.1f, 0.f ),
            Vector3F( 0.f, 0.2f, 0.f ), Vector3F( 0.1f, 0.2f, 0.f ), Vector3F( 0.2f, 0.2f, 0.f ), Vector3F( 0.3f, 0.2f, 0.f ),
            Vector3F( 0.f, 0.3f, 0.f ), Vector3F( 0.1f, 0.3f, 0.f ), Vector3F( 0.2f, 0.3f, 0.f ), Vector3F( 0.3f, 0.3f, 0.f )
        };
        const u16 cdistance_indices[] = 
        {
            0,1, 1,2, 2,3,
            4,5, 5,6, 6,7,
            8,9, 9,10, 10,11,
            12,13, 13,14, 14,15,

            0,4, 1,5, 2,6, 3,7,
            4,8, 5,9, 6,10, 7,11,
            8,12, 9,13, 10,14, 11,15,
        };
        const float particle_mass[16] =
        {
            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,
            0.f, 1.f, 0.f, 1.f,
        };

        PBDCloth::ActorDesc cloth_desc = {};
        cloth_desc.particle_mass = particle_mass;
        cloth_desc.positions = cloth_points;
        cloth_desc.cdistance_indices = cdistance_indices;
        cloth_desc.num_particles = 16;
        cloth_desc.num_cdistance_indices = sizeof( cdistance_indices ) / sizeof( *cdistance_indices );
        cloth_desc.num_particle_masses = 16;
        
        _cloth_id = _pbd_cloth_solver->CreateActor( cloth_desc );
    }

}

void Level::ShutDown( game_gfx::Deffered* gfx )
{
    BX_DELETE0( bxDefaultAllocator(), _pbd_cloth_solver );
    BX_DELETE0( bxDefaultAllocator(), _pbd_scene );

    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;
}

void Level::Tick( const GameTime& time )
{
    FluidColliders colliders;
    colliders.planes = &_plane_right;
    colliders.num_planes = 5;
    colliders.static_bodies = _boundary;
    colliders.num_static_bodies = 5;

    //FluidTick( &_fluid, _fluid_sim_params, colliders, time.DeltaTimeSec() );

    //StaticBodyDebugDraw( _boundary[0], 0x333333FF );
    //StaticBodyDebugDraw( _boundary[1], 0x333333FF );
    //StaticBodyDebugDraw( _boundary[2], 0x333333FF );
    //StaticBodyDebugDraw( _boundary[3], 0x333333FF );
    //StaticBodyDebugDraw( _boundary[4], 0x333333FF );

    //for( u32 i = 0; i < 5; ++i )
    //{
    //    const Vector4F plane = colliders.planes[i];

    //    const Vector3F point_on_plane = projectPointOnPlane( Vector3F( 0.f ), plane );

    //    const Vector3 pa( xyzw_to_m128( &point_on_plane.x ) );

    //    const Vector3F n = plane.getXYZ();
    //    const Vector3 v( xyzw_to_m128( &n.x ) );
    //    rdi::debug_draw::AddLine( pa, pa + v * 0.1f, 0xFF0000FF, 1 );
    //    rdi::debug_draw::AddSphere( Vector4( pa, _fluid.particle_radius ), 0xFF0000FF, 1 );

    //}
    Vector3F gravity( 0.f, -1.f, 0.f );
    _pbd_scene->PredictPosition( gravity, time.DeltaTimeSec() );
    _pbd_scene->UpdateSpatialMap();
    _pbd_cloth_solver->SolveConstraints();
    _pbd_scene->UpdateVelocity( time.DeltaTimeSec() );
    
    _pbd_cloth_solver->_DebugDraw( _cloth_id, 0xFF0000FF );

    if( ImGui::Begin( "Level" ) )
    {
        ImGui::BeginChild( "FluidParams", ImVec2(0,0), true );
            ImGui::InputFloat3( "gravity", &_fluid_sim_params.gravity.x, 3, ImGuiInputTextFlags_EnterReturnsTrue );
            ImGui::InputInt   ( "solverIterations", &_fluid_sim_params.solver_iterations );
            ImGui::InputFloat ( "timeStep", &_fluid_sim_params.time_step );
        ImGui::EndChild();
    }
    ImGui::End();
}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}


}
}//
