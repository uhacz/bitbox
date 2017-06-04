#include "puzzle_player.h"
#include "puzzle_player_internal.h"

#include <util\id_table.h>
#include <util\time.h>
#include <rdi\rdi_debug_draw.h>

#include "../imgui/imgui.h"

namespace bx { namespace puzzle {



struct PlayerData
{
    using IdTable = id_table_t<Const::MAX_PLAYERS>;
    
    IdTable          _id_table                        = {};
    Player           _id         [Const::MAX_PLAYERS] = {};
    PlayerName       _name       [Const::MAX_PLAYERS] = {};
    PlayerInput      _input      [Const::MAX_PLAYERS] = {};
    Matrix3F         _input_basis[Const::MAX_PLAYERS] = {};
    PlayerPose       _pose       [Const::MAX_PLAYERS] = {};
    Vector3F         _velocity   [Const::MAX_PLAYERS] = {};
    PlayerPoseBuffer _pose_buffer[Const::MAX_PLAYERS] = {};

    Vector3F _up_dir{ 0.f, 1.f, 0.f };

    u64 _time_us = 0;
    u64 _delta_time_us = 0;
    f32 _delta_time_acc = 0.f;

    struct Params
    {
        f32 move_speed = 10.f;
        f32 velocity_damping = 0.9f;
    }_params;

    bool IsAlive( id_t id ) const { return id_table::has( _id_table, id ); }

};
static PlayerData gData = {};

Player PlayerCreate( const char* name )
{
    if( id_table::size( gData._id_table ) == Const::MAX_PLAYERS )
        return { 0 };

    id_t id = id_table::create( gData._id_table );

    gData._id[id.index] = { id.hash };
    SetName( &gData._name[id.index], name );
    gData._pose[id.index] = {};
    gData._velocity[id.index] = Vector3F( 0.f );
    gData._input[id.index] = {};
    gData._input_basis[id.index] = Matrix3F::identity();
    Clear( &gData._pose_buffer[id.index] );

    return { id.hash };
}

void PlayerDestroy( Player pl )
{
    id_t id = { pl.i };
    if( !id_table::has( gData._id_table, id ) )
        return;

    gData._name[id.index].~PlayerName();
    
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

    void _PlayerWriteBuffer( u32 index )
    {
        const PlayerPose& pose = gData._pose[index];
        const PlayerInput& input = gData._input[index];
        const Matrix3F& basis = gData._input_basis[index];
        Write( &gData._pose_buffer[index], pose, input, basis, gData._time_us );
    }

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

    void _PlayerUpdate( u32 index, float deltaTimeS )
    {
        const PlayerInput& input = gData._input[index];
        const Matrix3F& basis = gData._input_basis[index];
        _PlayerUpdate( index, input, basis, deltaTimeS );
    }
}


void PlayerTick( u64 deltaTimeUS )
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

            if( ImGui::TreeNode( gData._name[i].str ) )
            {
                ImGui::Text( "speed: %.4f", length( gData._velocity[i] ) );
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
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

        PlayerPose render_pose;


        u32 back_index = BackIndex( gData._pose_buffer[id.index] );
        if( PeekPose( &render_pose, gData._pose_buffer[id.index], back_index ) )
        {
            const PlayerPose& pose = gData._pose[i];
            render_pose = Lerp( frameAlpha, render_pose, pose );
        }
        else
        {
            render_pose = gData._pose[i];
        }
        
        if( (i % 2) == 0 )
            rdi::debug_draw::AddSphere( Vector4F( render_pose.pos, rad ), 0xFF0000FF, 1 );
        else
            rdi::debug_draw::AddBox( Matrix4F( render_pose.rot, render_pose.pos ), Vector3F( rad ), 0xFF0000FF, 1 );

        rdi::debug_draw::AddAxes( appendScale( Matrix4F( render_pose.rot, render_pose.pos ), Vector3F(rad) ) );
        
    }
}

}}//