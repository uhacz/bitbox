#include "puzzle_player.h"
#include "puzzle_player_internal.h"

#include "puzzle_physics.h"
#include "puzzle_physics_util.h"
#include "puzzle_physics_gfx.h"

#include "puzzle_scene.h"

#include <util\id_table.h>
#include <util\array.h>
#include <util\time.h>
#include <util\poly\par_shapes.h>
#include <rdi\rdi_debug_draw.h>

#include "../imgui/imgui.h"

namespace bx { namespace puzzle {



struct PlayerData
{
    using IdTable = id_table_t<Const::MAX_PLAYERS>;
    
    IdTable          _id_table                        = {};
    Player           _id               [Const::MAX_PLAYERS] = {};
    char*            _name             [Const::MAX_PLAYERS] = {};
    PlayerInput      _input            [Const::MAX_PLAYERS] = {};
    Matrix3F         _input_basis      [Const::MAX_PLAYERS] = {};
    PlayerPose       _pose             [Const::MAX_PLAYERS] = {};
    Vector3F         _velocity         [Const::MAX_PLAYERS] = {};
    PlayerPose       _interpolated_pose[Const::MAX_PLAYERS] = {};
    PlayerPoseBuffer _pose_buffer      [Const::MAX_PLAYERS] = {};
    physics::BodyId  _body_id          [Const::MAX_PLAYERS] = {};

    Vector3F _up_dir{ 0.f, 1.f, 0.f };

    u64 _time_us = 0;
    u64 _delta_time_us = 0;
    f32 _delta_time_acc = 0.f;

    struct Params
    {
        f32 move_speed = 10.f;
        f32 velocity_damping = 0.9f;
        f32 radius = 0.75f;
    }_params;

    bool IsAlive( id_t id ) const { return id_table::has( _id_table, id ); }

};
static PlayerData gData = {};

static inline Vector3F GetPlayerCenterPos( u32 index )
{
    const PlayerPose& pp = gData._interpolated_pose[index];
    const Vector3F offset = gData._up_dir * gData._params.radius;
    return pp.pos + offset;
}

static physics::BodyId CreatePlayerPhysics( SceneCtx* sctx, const PlayerPose& pp, float radius )
{
    physics::Solver* solver = sctx->phx_solver;

    par_shapes_mesh* mesh   = par_shapes_create_subdivided_sphere( 1 );
    //par_shapes_mesh* mesh = par_shapes_create_parametric_sphere( 16, 16 );
    //par_shapes_mesh* mesh = par_shapes_create_octohedron();
    physics::BodyId body_id = physics::CreateBody( solver, mesh->npoints );

    const Vector3F* mesh_points = (Vector3F*)mesh->points;
    Vector3F* body_points       = physics::MapPosition( solver, body_id );
    f32* body_w                 = physics::MapMassInv( solver, body_id );

    for( int i = 0; i < mesh->npoints; ++i )
    {
        const Vector3F& pos_ls = mesh_points[i] * radius;
        const Vector3F pos_ws = fastTransform( pp.rot, pp.pos, pos_ls );

        body_points[i] = pos_ws;
        body_w[i] = 0.1f;
    }
    
    physics::Unmap( solver, body_w );
    physics::Unmap( solver, body_points );

    physics::CalculateLocalPositions( solver, body_id, 0.51f );

    array_t< physics::DistanceCInfo >c_info;
    for( int i = 0; i < mesh->ntriangles; ++i )
    {
        const int base_i = i * 3;
        const u16 i0 = mesh->triangles[base_i+0];
        const u16 i1 = mesh->triangles[base_i+1];
        const u16 i2 = mesh->triangles[base_i+2];

        physics::DistanceCInfo c;
        
        c.i0 = i0; c.i1 = i1;
        array::push_back( c_info, c );

        c.i0 = i1; c.i1 = i2;
        array::push_back( c_info, c );

        c.i0 = i0; c.i1 = i2;
        array::push_back( c_info, c );
    }
    physics::SetDistanceConstraints( solver, body_id, c_info.begin(), c_info.size );
    physics::SetBodySelfCollisions( solver, body_id, false );

    par_shapes_free_mesh( mesh );

    physics::SetFriction( solver, body_id, physics::FrictionParams( 0.05f, 0.9f ) );
    physics::SetVelocityDamping( solver, body_id, 0.5f );

    return body_id;
}

Player PlayerCreate( SceneCtx* sctx, const char* name, const Matrix4F& pose )
{
    if( id_table::size( gData._id_table ) == Const::MAX_PLAYERS )
        return { 0 };

    id_t id = id_table::create( gData._id_table );

    gData._id[id.index]                = { id.hash };
    gData._name[id.index]              = string::duplicate( gData._name[id.index], name );
    gData._pose[id.index]              = PlayerPose::fromMatrix4( pose );
    gData._interpolated_pose[id.index] = gData._pose[id.index];
    gData._velocity[id.index]          = Vector3F( 0.f );
    gData._input[id.index]             = {};
    gData._input_basis[id.index]       = Matrix3F::identity();
    Clear( &gData._pose_buffer[id.index] );

    //physics::BodyId body_id = physics::CreateSphere( sctx->phx_solver, pose, 0.75f, 1.0f );
    physics::BodyId body_id = CreatePlayerPhysics( sctx, gData._pose[id.index], gData._params.radius );
    physics::AddBody( sctx->phx_gfx, body_id );
    physics::SetColor( sctx->phx_gfx, body_id, 0xFFFFFFFF );
    gData._body_id[id.index] = body_id;

    return { id.hash };
}

void PlayerDestroy( Player pl )
{
    id_t id = { pl.i };
    if( !id_table::has( gData._id_table, id ) )
        return;

    string::free_and_null( &gData._name[id.index] );
    id_table::destroy( gData._id_table, id );
}

bool IsAlive( Player pl )
{
    id_t id = { pl.i };
    return gData.IsAlive( id );
}

void PlayerCollectInput( Player pl, const bxInput& input, const Matrix3F& basis, u64 deltaTimeUS )
{
    id_t id = { pl.i };
    if( !gData.IsAlive( id ) )
        return;


    const float delta_time_s = (float)bxTime::toSeconds( gData._delta_time_us );

    PlayerInput& playerInput = gData._input[id.index];
    Collect( &playerInput, input, delta_time_s );
    
    gData._input[id.index] = playerInput;
    gData._input_basis[id.index] = basis;
}

namespace
{
    Vector3F _ComputePlayerLocalMoveVector( const PlayerInput& input )
    {
        Vector3F move_vector{ 0.f };
        move_vector += Vector3F::xAxis() * input.analogX;
        move_vector -= Vector3F::zAxis() * input.analogY;
        return move_vector;
    }

    Vector3F _ComputePlayerWorldMoveVector( const Vector3F& moveVectorLS, const Matrix3F& basis, const Vector3F& upDir )
    {
        const float move_vector_len = length( moveVectorLS );
        const Vector3F move_vector_ws = basis * moveVectorLS;
        const Vector3F move_vector_ws_flat = normalizeSafe( projectVectorOnPlane( move_vector_ws, upDir ) ) * move_vector_len;
        return move_vector_ws_flat;
    }

    static void _SplitVelocityXZ_Y( Vector3F* velXZ, Vector3F* velY, const Vector3F& vel, const Vector3F& upDir )
    {
        velXZ[0] = projectVectorOnPlane( vel, upDir );
        velY[0] = vel - velXZ[0];
    }
    static Vector3F _ApplySpeedLimitXZ( const Vector3F& vel, const Vector3F& upDir, float spdLimit )
    {
        Vector3F velXZ, velY;
        _SplitVelocityXZ_Y( &velXZ, &velY, vel, upDir );
        {
            const float spd = length( velXZ );
            const float velScaler = ( spdLimit / spd );
            const float a = ( spd > spdLimit ) ? velScaler : 1.f;
            velXZ *= a;
        }
        return velXZ + velY;
    }

    static Vector3F _ApplySpeedLimit( const Vector3F& vel, float spdLimit )
    {
        const float spd = length( vel );
        const float velScaler = ( spdLimit / spd );
        const float a = ( spd > spdLimit ) ? velScaler : 1.f;
        return vel * a;
    }

    void _PlayerWriteBuffer( u32 index )
    {
        const PlayerPose& pose = gData._pose[index];
        const PlayerInput& input = gData._input[index];
        const Matrix3F& basis = gData._input_basis[index];
        Write( &gData._pose_buffer[index], pose, input, basis, gData._time_us );
    }

#if 0
    static void _PlayerDrivePhysics( u32 i, physics::Solver* solver, float deltaTimeS )
    {
        physics::BodyId body_id = gData._body_id[i];
        const u32 n_particles = physics::GetNbParticles( solver, body_id );
        const f32 pradius = physics::GetParticleRadius( solver );

        const Vector3F center_pos = GetPlayerCenterPos( i );
        const Vector3F dst_pos = center_pos;// + gData._up_dir * pradius * 0.9f;
        const physics::BodyCoM phx_com = physics::GetBodyCoM( solver, body_id );
        const Vector3F f = ( dst_pos - phx_com.pos );

        Vector3F* vel = physics::MapVelocity( solver, body_id );

        const float player_speed = length( gData._velocity[i] );
        const float dist_to_travel = length( f );
        for( u32 iparticle = 0; iparticle < n_particles; ++iparticle )
        {
            Vector3F v = vel[iparticle];
            v += f;

            if( length( v*deltaTimeS ) > dist_to_travel )
                v = normalize( v ) * dist_to_travel;

            vel[iparticle] = v;
        }

        physics::Unmap( solver, vel );
    }
#endif
    

    void _PlayerUpdate( u32 index, const PlayerInput& input, const Matrix3F& basis, float deltaTimeS )
    {
        const float velocity_damping = ::powf( 1.f - gData._params.velocity_damping, deltaTimeS );

        Vector3F move_vec_ls = _ComputePlayerLocalMoveVector( input );
        Vector3F move_vec_ws = _ComputePlayerWorldMoveVector( move_vec_ls, basis, gData._up_dir );

        // --- this value is form 0 to 1 range
        const float move_vec_ls_len = minOfPair( length( move_vec_ls ), 1.f );
        const float curr_move_speed = gData._params.move_speed * move_vec_ls_len;

        const Vector3F curr_vel = gData._velocity[index];
        const Vector3F acc = ( move_vec_ws * curr_move_speed );

        Vector3F vel = curr_vel + acc * deltaTimeS;

        // --- velocity damping
        if( lengthSqr( move_vec_ws ) <= 0.1f )
        {
            vel *= velocity_damping;
        }
        else
        {
            const float d = maxOfPair( 0.f, dot( normalizeSafe( curr_vel ), normalizeSafe( acc ) ) );
            const float damping = lerp( d*d, velocity_damping, 1.f );
            vel *= damping;
        }

        vel = _ApplySpeedLimitXZ( vel, gData._up_dir, curr_move_speed );

        gData._pose[index].pos += vel * deltaTimeS;
        gData._velocity[index] = vel;
    }

    static void _PlayerInterpolatePose( u32 index )
    {
        const float frameAlpha = gData._delta_time_acc / Const::FIXED_DT;
        
        PlayerPose interpolated_pose;
        u32 back_index = BackIndex( gData._pose_buffer[index] );
        if( PeekPose( &interpolated_pose, gData._pose_buffer[index], back_index ) )
        {
            const PlayerPose& pose = gData._pose[index];
            interpolated_pose = Lerp( frameAlpha, interpolated_pose, pose );
        }
        else
        {
            interpolated_pose = gData._pose[index];
        }

        gData._interpolated_pose[index] = interpolated_pose;
    }

    static void _PlayerDrivePhysics( u32 i, physics::Solver* solver, float deltaTimeS )
    {
        PlayerInput pin;
        Matrix3F basis;
        u32 back_index = BackIndex( gData._pose_buffer[i] );
        if( PeekInput( &pin, &basis, gData._pose_buffer[i], back_index ) )
        {
            Vector3F move_vec_ls = _ComputePlayerLocalMoveVector( pin );
            Vector3F move_vec_ws = _ComputePlayerWorldMoveVector( move_vec_ls, basis, gData._up_dir );

            // --- this value is form 0 to 1 range
            const float move_vec_ls_len = minOfPair( length( move_vec_ls ), 1.f );
            const float curr_move_speed = gData._params.move_speed * move_vec_ls_len;

            const Vector3F acc = ( move_vec_ws * curr_move_speed );
            physics::BodyId body_id = gData._body_id[i];
            //physics::SetExternalForce( solver, body_id, acc );
            const u32 n_particles = physics::GetNbParticles( solver, body_id );
            Vector3F* vel = physics::MapVelocity( solver, body_id );

            //const float player_speed = length( gData._velocity[i] );
            //const float dist_to_travel = length( acc );
            for( u32 iparticle = 0; iparticle < n_particles; ++iparticle )
            {
                Vector3F v = vel[iparticle];
                v += acc * deltaTimeS;

                //if( length( v*deltaTimeS ) > dist_to_travel )
                //    v = normalize( v ) * dist_to_travel;

                vel[iparticle] = v;
            }

            physics::Unmap( solver, vel );
        }
    }

    void _PlayerUpdate( u32 index, float deltaTimeS )
    {
        const PlayerInput& input = gData._input[index];
        const Matrix3F& basis    = gData._input_basis[index];
        _PlayerUpdate( index, input, basis, deltaTimeS );
    }
}


void PlayerTick( SceneCtx* sctx, u64 deltaTimeUS )
{
    //TODO: 
    //      shape physics
    //      collisions

    u32 alive_indices[Const::MAX_PLAYERS] = {};
    u32 num_alive = 0;
    for( u32 i = 0; i < Const::MAX_PLAYERS; ++i )
    {
        id_t id = { gData._id[i].i };
        if( !gData.IsAlive( id ) )
            continue;
        
        SYS_ASSERT( num_alive < Const::MAX_PLAYERS );
        alive_indices[num_alive++] = id.index;
    }

    const float delta_time_s = (float)bxTime::toSeconds( gData._delta_time_us );
    //const float delta_time_srcp = ( delta_time_s > FLT_EPSILON ) ? 1.f / delta_time_s : 0.f;

    gData._delta_time_acc += delta_time_s;
    u32 iteration_count = 0;
    while( gData._delta_time_acc >= Const::FIXED_DT )
    {
        ++iteration_count;
        gData._delta_time_acc -= Const::FIXED_DT;
    }

    for( u32 ialive = 0; ialive < num_alive; ++ialive )
    {
        const u32 i = alive_indices[ialive];
        for( u32 it = 0; it < iteration_count; ++it )
        {
            _PlayerWriteBuffer( i );
            _PlayerUpdate( i, Const::FIXED_DT );
        }

        _PlayerInterpolatePose( i );
        _PlayerDrivePhysics( i, sctx->phx_solver, delta_time_s );

    }

    gData._delta_time_us = deltaTimeUS;
    gData._time_us += deltaTimeUS;



    if( ImGui::Begin( "Player" ) )
    {
        ImGui::SliderFloat( "move speed", &gData._params.move_speed, 0.f, 100.f );
        ImGui::SliderFloat( "velocity damping", &gData._params.velocity_damping, 0.f, 1.f );

        for( u32 ialive = 0; ialive < num_alive; ++ialive )
        {
            const u32 i = alive_indices[ialive];

            if( ImGui::TreeNode( gData._name[i] ) )
            {
                ImGui::Text( "speed: %.4f", length( gData._velocity[i] ) );
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

void PlayerPostPhysicsTick( SceneCtx* sctx )
{
    const float solver_frequency = physics::GetFrequency( sctx->phx_solver );

    for( u32 i = 0; i < Const::MAX_PLAYERS; ++i )
    {
        id_t id = { gData._id[i].i };
        if( !gData.IsAlive( id ) )
            continue;
    
        const u32 index = id.index;

        const physics::BodyId body_id    = gData._body_id[index];
        const physics::BodyCoM com       = physics::GetBodyCoM( sctx->phx_solver, body_id );
        const physics::BodyCoM com_displ = physics::GetBodyCoMDisplacement( sctx->phx_solver, body_id );
        
        const Vector3F body_vel = com_displ.pos * solver_frequency;
        Vector3F player_vel     = gData._velocity[index];

        const float body_spd   = length( body_vel );
        const float player_spd = length( player_vel );

        if( body_spd < player_spd*0.5f )
            player_vel = normalizeSafe( player_vel ) * body_spd;

        gData._velocity[index] = player_vel;
    }
}

void PlayerDraw( Player pl )
{
    const float rad = 0.5f;

    const float frameAlpha = gData._delta_time_acc / Const::FIXED_DT;

    for( u32 i = 0; i < Const::MAX_PLAYERS; ++i )
    {
        id_t id = { gData._id[i].i };
        if( !gData.IsAlive( id ) )
            continue;

        const PlayerPose& render_pose = gData._interpolated_pose[id.index];
        
        if( (i % 2) == 0 )
            rdi::debug_draw::AddSphere( Vector4F( render_pose.pos, rad ), 0xFF0000FF, 1 );
        else
            rdi::debug_draw::AddBox( Matrix4F( render_pose.rot, render_pose.pos ), Vector3F( rad ), 0xFF0000FF, 1 );

        rdi::debug_draw::AddAxes( appendScale( Matrix4F( render_pose.rot, render_pose.pos ), Vector3F(rad) ) );
        
    }
}

}}//