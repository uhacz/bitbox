#include "puzzle_player.h"
#include <util\vectormath\vectormath.h>
#include <util\id_table.h>
#include <util\common.h>
#include <util\ring_buffer.h>
#include <util\string_util.h>
#include <system\input.h>
#include "util\signal_filter.h"
#include "util\time.h"

namespace bx { namespace puzzle {

namespace Const
{
    static const u8 FRAMERATE = 60;
    static const u8 MAX_PLAYERS = 2;
    static const u8 POSE_BUFFER_SIZE = 16;
    static const u8 POSE_BUFFER_LEN_MS = (u8)( (double)POSE_BUFFER_SIZE * (1.0/double(FRAMERATE) ) );
}//

//////////////////////////////////////////////////////////////////////////
struct PlayerName
{
    char* str = nullptr;

    ~PlayerName()
    {
        BX_FREE0( bxDefaultAllocator(), str );
    }
};
void SetName( PlayerName* pn, const char* str )
{
    pn->str = string::duplicate( pn->str, str );
}

//////////////////////////////////////////////////////////////////////////
struct PlayerInput
{
    f32 analogX = 0.f;
    f32 analogY = 0.f;
    f32 jump = 0.f;
    f32 crouch = 0.f;
    f32 L2 = 0.f;
    f32 R2 = 0.f;
};

//////////////////////////////////////////////////////////////////////////
struct PlayerPose
{
    Vector3F pos{0.f};
    QuatF    rot = QuatF::identity();
};
inline PlayerPose Lerp( float t, const PlayerPose& a, const PlayerPose& b )
{
    PlayerPose p;
    p.pos = lerp( t, a.pos, b.pos );
    p.rot = slerp( t, a.rot, b.rot );
    return p;
}

//////////////////////////////////////////////////////////////////////////
struct PlayerPoseBuffer
{
    PlayerPose poses[Const::POSE_BUFFER_SIZE] = {};
    u64        timestamp[Const::POSE_BUFFER_SIZE] = {};

    using Ring = ring_t < Const::POSE_BUFFER_SIZE >;
    Ring _ring;
};


void Clear( PlayerPoseBuffer* ppb )
{
    ppb->_ring.clear();
}
u32 Write( PlayerPoseBuffer* ppb, const PlayerPose& pose, u64 ts )
{
    if( ppb->_ring.full() )
        ppb->_ring.shift();

    u32 index = ppb->_ring.push();
    ppb->poses[index] = pose;
    ppb->timestamp[index] = ts;

    return index;
}
bool Read( PlayerPose* pose, u64* ts, PlayerPoseBuffer* ppb )
{
    if( ppb->_ring.empty() )
        return false;

    u32 index = ppb->_ring.shift();
    pose[0] = ppb->poses[index];
    ts[0] = ppb->timestamp[index];

    return true;
}
bool Peek( PlayerPose* pose, u64* ts, const PlayerPoseBuffer& ppb )
{
    u32 index;
    if( !ppb._ring.peek( &index ) )
        return false;

    pose[0] = ppb.poses[index];
    ts[0] = ppb.timestamp[index];

    return true;
}

struct PlayerData
{
    using IdTable = id_table_t<Const::MAX_PLAYERS>;
    
    IdTable          _id_table                        = {};
    Player           _id         [Const::MAX_PLAYERS] = {};
    PlayerPose       _pose       [Const::MAX_PLAYERS] = {};
    PlayerPoseBuffer _pose_buffer[Const::MAX_PLAYERS] = {};
    PlayerName       _name       [Const::MAX_PLAYERS] = {};
    PlayerInput      _input      [Const::MAX_PLAYERS] = {};

    Vector3F _up_dir{ 0.f, 1.f, 0.f };

    u64 _time_us = 0;
    u64 _delta_time_us = 0;

    bool IsAlive( id_t id ) const { return id_table::has( _id_table, id ); }

};
static PlayerData gData = {};

Player PlayerCreate( const char* name )
{
    if( id_table::size( gData._id_table ) == Const::MAX_PLAYERS )
        return { 0 };

    id_t id = id_table::create( gData._id_table );

    SetName( &gData._name[id.index], name );
    gData._id[id.index] = { id.hash };
    return { id.hash };
}

void PlayerDestroy( Player pl )
{
    id_t id = { pl.i };
    if( !id_table::has( gData._id_table, id ) )
        return;

    id_table::destroy( gData._id_table, id );
}

namespace
{
void _CollectInput( PlayerInput* playerInput, const bxInput& input, float deltaTime )
{
    float analogX = 0.f;
    float analogY = 0.f;
    float jump = 0.f;
    float crouch = 0.f;
    float L2 = 0.f;
    float R2 = 0.f;

    const bxInput_Pad& pad = bxInput_getPad( input );
    const bxInput_PadState* padState = pad.currentState();
    if( padState->connected )
    {
        analogX = padState->analog.left_X;
        analogY = padState->analog.left_Y;

        crouch = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCIRCLE );
        jump = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCROSS );

        L2 = padState->analog.L2;
        R2 = padState->analog.R2;
    }
    else
    {
        const bxInput_Keyboard* kbd = &input.kbd;

        const int inLeft = bxInput_isKeyPressed( kbd, 'A' );
        const int inRight = bxInput_isKeyPressed( kbd, 'D' );
        const int inFwd = bxInput_isKeyPressed( kbd, 'W' );
        const int inBack = bxInput_isKeyPressed( kbd, 'S' );
        const int inJump = bxInput_isKeyPressedOnce( kbd, ' ' );
        const int inCrouch = bxInput_isKeyPressedOnce( kbd, bxInput::eKEY_LSHIFT );
        const int inL2 = bxInput_isKeyPressed( kbd, 'Q' );
        const int inR2 = bxInput_isKeyPressed( kbd, 'E' );

        analogX = -(float)inLeft + (float)inRight;
        analogY = -(float)inBack + (float)inFwd;

        crouch = (f32)inCrouch;
        jump = (f32)inJump;

        L2 = (float)inL2;
        R2 = (float)inR2;

        //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
    }

    const float RC = 0.01f;
    playerInput->analogX = signalFilter_lowPass( analogX, playerInput->analogX, RC, deltaTime );
    playerInput->analogY = signalFilter_lowPass( analogY, playerInput->analogY, RC, deltaTime );
    playerInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
    playerInput->crouch = signalFilter_lowPass( crouch, playerInput->crouch, RC, deltaTime );
    playerInput->L2 = L2;
    playerInput->R2 = R2;
}
}//

void PlayerMove( Player pl, const bxInput& input, const Matrix3F& basis )
{
    id_t id = { pl.i };
    if( !gData.IsAlive( id ) )
        return;

    const float delta_time_s = (float)bxTime::toSeconds( gData._delta_time_us );

    PlayerInput& playerInput = gData._input[id.index];
    _CollectInput( &playerInput, input, delta_time_s );

    Vector3F move_vector{ 0.f };
    move_vector += Vector3F::xAxis() * playerInput.analogX;
    move_vector += Vector3F::zAxis() * playerInput.analogY;

    float move_vector_len = length( move_vector );
    move_vector = normalize( projectVectorOnPlane( move_vector, gData._up_dir ) ) * move_vector_len;

    PlayerPose& pose = gData._pose[id.index];
    pose.pos += move_vector * delta_time_s;
}

void PlayerTick( u64 deltaTimeUS )
{
    for( u32 i = 0; i < Const::MAX_PLAYERS; ++i )
    {
        id_t id = { gData._id[i].i };
        if( !gData.IsAlive( id ) )
            continue;

        const PlayerPose& pose = gData._pose[i];
        Write( &gData._pose_buffer[i], pose, gData._time_us );
    }
    
    gData._delta_time_us = deltaTimeUS;
    gData._time_us += deltaTimeUS;
}

void PlayerDraw( Player pl )
{

}

}}//