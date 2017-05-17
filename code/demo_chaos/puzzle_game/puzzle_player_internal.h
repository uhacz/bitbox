#pragma once

#include <util/vectormath/vectormath.h>
#include <util/memory.h>
#include <util/string_util.h>
#include <util/ring_buffer.h>
#include <util/signal_filter.h>

#include <system/input.h>

namespace bx{ namespace puzzle {


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
void SetName( PlayerName* pn, const char* str );

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
void Collect( PlayerInput* playerInput, const bxInput& input, float deltaTime );

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
    PlayerPose  poses    [Const::POSE_BUFFER_SIZE] = {};
    PlayerInput input    [Const::POSE_BUFFER_SIZE] = {};
    Matrix3F    basis    [Const::POSE_BUFFER_SIZE] = {};
    u64         timestamp[Const::POSE_BUFFER_SIZE] = {};

    using Ring = ring_t < Const::POSE_BUFFER_SIZE >;
    Ring _ring;
};

void Clear( PlayerPoseBuffer* ppb );
u32  Write( PlayerPoseBuffer* ppb, const PlayerPose& pose, const PlayerInput& input, const Matrix3F& basis, u64 ts );
bool Read( PlayerPose* pose, PlayerInput* input, Matrix3F* basis, u64* ts, PlayerPoseBuffer* ppb );
}}//