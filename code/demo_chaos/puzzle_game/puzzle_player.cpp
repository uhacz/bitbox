#include "puzzle_player.h"
#include "puzzle_player_internal.h"

#include <util\id_table.h>
#include <util\time.h>
#include "rdi\rdi_debug_draw.h"

namespace bx { namespace puzzle {



struct PlayerData
{
    using IdTable = id_table_t<Const::MAX_PLAYERS>;
    
    IdTable          _id_table                        = {};
    Player           _id         [Const::MAX_PLAYERS] = {};
    PlayerName       _name       [Const::MAX_PLAYERS] = {};
    PlayerPose       _pose       [Const::MAX_PLAYERS] = {};
    PlayerInput      _input      [Const::MAX_PLAYERS] = {};
    Matrix3F         _input_basis[Const::MAX_PLAYERS] = {};
    PlayerPoseBuffer _pose_buffer[Const::MAX_PLAYERS] = {};

    Vector3F _up_dir{ 0.f, 1.f, 0.f };

    u64 _time_us = 0;
    u64 _delta_time_us = 0;

    struct Params
    {
        f32 move_speed = 3.f;
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
}

void PlayerTick( u64 deltaTimeUS )
{
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

    for( u32 ialive = 0; ialive < num_alive; ++ialive )
    {
        const u32 i = alive_indices[ialive];
        PlayerPose& pose = gData._pose[i];
        const PlayerInput& input = gData._input[i];
        const Matrix3F& basis = gData._input_basis[i];
        Write( &gData._pose_buffer[i], pose, input, basis, gData._time_us );

        Vector3F move_vec_ls = _ComputePlayerLocalMoveVector( input );
        Vector3F move_vec_ws = _ComputePlayerWorldMoveVector( move_vec_ls, basis, gData._up_dir );

        pose.pos += move_vec_ws * delta_time_s * gData._params.move_speed;
    }
    
    gData._delta_time_us = deltaTimeUS;
    gData._time_us += deltaTimeUS;
}

void PlayerDraw( Player pl )
{
    const float rad = 0.5f;
    for( u32 i = 0; i < Const::MAX_PLAYERS; ++i )
    {
        id_t id = { gData._id[i].i };
        if( !gData.IsAlive( id ) )
            continue;

        const PlayerPose& pose = gData._pose[i];
        
        if( (i % 2) == 0 )
            rdi::debug_draw::AddSphere( Vector4F( pose.pos, rad ), 0xFF0000FF, 1 );
        else
            rdi::debug_draw::AddBox( Matrix4F(pose.rot, pose.pos ), Vector3F( rad ), 0xFF0000FF, 1 );

        rdi::debug_draw::AddAxes( appendScale( Matrix4F( pose.rot, pose.pos ), Vector3F(rad) ) );
        
    }
}

}}//