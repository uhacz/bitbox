#include "puzzle_player.h"
#include <util\vectormath\vectormath.h>
#include <util\id_table.h>
#include <util\common.h>

namespace bx { namespace puzzle {

namespace Const
{
    static const u8 FRAMERATE = 60;
    static const u8 MAX_PLAYERS = 2;
    static const u8 POSE_BUFFER_SIZE = 16;
    static const u8 POSE_BUFFER_LEN_MS = (u8)( (double)POSE_BUFFER_SIZE * (1.0/double(FRAMERATE) ) );
}//

struct PlayerPose
{
    Vector3F pos{0.f};
    QuatF    rot = QuatF::identity();
};
inline PlayerPose lerp( float t, const PlayerPose& a, const PlayerPose& b )
{
    PlayerPose p;
    p.pos = lerp( t, a.pos, b.pos );
    p.rot = slerp( t, a.rot, b.rot );
    return p;
}

template< u32 CAPACITY /*must be power of 2*/ >
struct ring_t
{
    typedef ring_t<CAPACITY> ring_type;

    u32 _read = 0;
    u32 _write = 0;

    ring_t()
    {
        // check if CAPACITY is power of 2
        SYS_STATIC_ASSERT( ( ( CAPACITY != 0 ) && ( ( CAPACITY  & ( ~CAPACITY + 1 ) ) == CAPACITY ) ) );
    }

    static inline u32 _mask ( u32 val ) { return val & ( CAPACITY - 1 ); }
    
    u32 push () { SYS_ASSERT( !full() ); return _mask( _write++ ); }
    u32 shift() { SYS_ASSERT( !empty() ); return _mask( _read++ ); }
    bool peek( u32* i ) const
    {
        if( empty() )
            return false;

        i[0] = _read;
        return true;
    }
    
    bool empty() const { return _read == _write; }
    bool full () const { return size() == CAPACITY; }
    u32  size () const { return _write - _read; }
    void clear() { _write = _read = 0; }

    struct iterator
    {
        ring_type& _ring;
        u32 _it;
        iterator( ring_type& r ): _ring(r), _it( 0 )
        {}

        const u32 operator * () const { return _mask( _ring._read + _it ); }
        void next() { ++_it; }
        void done() { _read._write <= _it; }
    };
};

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
        return UINT32_MAX;

    u32 index = ppb->_ring.push();
    ppb->poses[index] = pose;
    ppb->timestamp[index] = ts;

    return index;
}
bool Read( PlayerPose* pose, u32* ts, PlayerPoseBuffer* ppb )
{
    if( ppb->_ring.empty() )
        return false;

    u32 index = ppb->_ring.shift();
    pose[0] = ppb->poses[index];
    ts[0] = ppb->timestamp[index];

    return true;
}
bool Peek( PlayerPose* pose, u32* ts, const PlayerPoseBuffer& ppb )
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
    
    IdTable  _id_table = {};
    PlayerPoseBuffer _pose_buffer[Const::MAX_PLAYERS];
};

}}//