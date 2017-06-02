#include "puzzle_player_internal.h"

namespace bx{ namespace puzzle {

//////////////////////////////////////////////////////////////////////////
void SetName( PlayerName* pn, const char* str )
{
    pn->str = string::duplicate( pn->str, str );
}

//////////////////////////////////////////////////////////////////////////
void Collect( PlayerInput* playerInput, const bxInput& input, float deltaTime )
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

        //bxLogInfo( "x: %f, y: %f", analogX, analogX );
    }

    const float RC = 0.1f;
    playerInput->analogX = signalFilter_lowPass( analogX, playerInput->analogX, RC, deltaTime );
    playerInput->analogY = signalFilter_lowPass( analogY, playerInput->analogY, RC, deltaTime );
    playerInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
    playerInput->crouch = signalFilter_lowPass( crouch, playerInput->crouch, RC, deltaTime );
    playerInput->L2 = L2;
    playerInput->R2 = R2;
}

//////////////////////////////////////////////////////////////////////////
void Clear( PlayerPoseBuffer* ppb )
{
    ppb->_ring.clear();
}

// ---
u32 Write( PlayerPoseBuffer* ppb, const PlayerPose& pose, const PlayerInput& input, const Matrix3F& basis, u64 ts )
{
    if( ppb->_ring.full() )
        ppb->_ring.shift();

    u32 index = ppb->_ring.push();

    ppb->poses[index] = pose;
    ppb->input[index] = input;
    ppb->basis[index] = basis;
    ppb->timestamp[index] = ts;

    return index;
}
// ---
bool Read( PlayerPose* pose, PlayerInput* input, Matrix3F* basis, u64* ts, PlayerPoseBuffer* ppb )
{
    if( ppb->_ring.empty() )
        return false;

    u32 index = ppb->_ring.shift();

    pose[0] = ppb->poses[index];
    input[0] = ppb->input[index];
    basis[0] = ppb->basis[index];
    ts[0] = ppb->timestamp[index];

    return true;
}

bool Peek( PlayerPose* pose, PlayerInput* input, Matrix3F* basis, u64* ts, const PlayerPoseBuffer& ppb, u32 index )
{
    if( index >= ppb._ring.capacity() )
        return false;

    pose[0]  = ppb.poses[index];
    input[0] = ppb.input[index];
    basis[0] = ppb.basis[index];
    ts[0]    = ppb.timestamp[index];

    return true;

}

bool PeekPose( PlayerPose* pose, const PlayerPoseBuffer& ppb, u32 index )
{
    if( index >= ppb._ring.capacity() )
        return false;

    pose[0] = ppb.poses[index];

    return true;
}

u32 BackIndex( const PlayerPoseBuffer& ppb )
{
    u32 back_index = ppb._ring._write;
    back_index = wrap_dec_u32( back_index, 0, UINT32_MAX );
    return ppb._ring._mask( back_index );
}

}}//


#include <util/array.h>
#include <util/id_table.h>

namespace bx { namespace puzzle {

namespace physics
{
namespace EBody
{
    enum E
    {
        eSOFT = 0,
        eCLOTH,
        eROPE,
        _COUNT_,
    };
}//

namespace EConst
{
    enum E
    {
        eMAX_BODIES = 64,
        eMAX_OBJECTS = eMAX_BODIES * EBody::_COUNT_,
    };
}//



// --- 
using Vector3Array = array_t<Vector3F>;
using F32Array = array_t<f32>;
using U32Array = array_t<u32>;

    
    
// --- constraints
struct DistanceC
{
    u32 i0;
    u32 i1;
    f32 rl;
};

struct BendingC
{
    u32 i0;
    u32 i1;
    u32 i2;
    u32 i3;
    f32 ra;
};

struct RestPositionC
{
    Vector3F rest_pos;
    u32 particle_index;
};

using DistanceCArray = array_t<DistanceC>;
using BendingCArray  = array_t<BendingC>;

struct PhysicsParams
{
    f32 vel_damping      = 0.1f;
    f32 static_friction  = 0.1f;
    f32 dynamic_friction = 0.1f;
    f32 restitution      = 0.2f;
};

// --- bodies
struct PhysicsBody
{
    u32 begin = 0;
    u32 count = 0;
};
inline bool IsValid( const PhysicsBody& body ) { return body.count != 0; }

struct PhysicsSoftBody
{
    //PhysicsBody body;
    //PhysicsParams params;
    u32 rest_pos_begin = 0;
    u32 rest_pos_count = 0;

    Vector3Array   rest_p;
    DistanceCArray distance_c;
};
struct PhysicsClothBody
{
    //PhysicsBody body;
    //PhysicsParams params;
    DistanceCArray distance_c;
};
struct PhysicsBodyRope
{
    //PhysicsBody body;
    //PhysicsParams params;
    DistanceCArray distance_c;
};

// --- body id
union BodyIdInternal
{
    u32 i;
    struct
    {
        u32 id    : 16;
        u32 index : 12;
        u32 type  : 4; // EBody
    };
};
inline BodyId         ToBodyId        ( BodyIdInternal idi ) { return{ idi.i }; }
inline BodyIdInternal ToBodyIdInternal( BodyId id )          { return{ id.i }; }
using IdTable = id_table_t< EConst::eMAX_BODIES, BodyIdInternal >;
using PhysicsBodyArray = array_t<PhysicsBody>;


// --- solver
struct Solver
{
    Vector3Array p0;
    Vector3Array p1;
    Vector3Array v;
    F32Array     w;
        
    DistanceCArray   distance_c;

    IdTable          id_tbl    [EBody::_COUNT_];
    PhysicsBody      bodies    [EBody::_COUNT_][EConst::eMAX_BODIES];
    PhysicsParams    params    [EBody::_COUNT_][EConst::eMAX_BODIES];

    PhysicsSoftBody  soft_body [EConst::eMAX_BODIES];
    PhysicsClothBody cloth_body[EConst::eMAX_BODIES];
    PhysicsBodyRope  rope_body [EConst::eMAX_BODIES];

    BodyIdInternal   active_bodies_idi[EConst::eMAX_OBJECTS] = {};
    u16              active_bodies_count = 0;

    PhysicsBodyArray _to_deallocate;

    u32 num_iterations = 4;
    u32 frequency = 60;
    f32 delta_time = 1.f / frequency;

    union Flags
    {
        u32 all = 0;
        struct
        {
            u32 rebuild_constraints : 1;
        };
    } flags;


    u32 Capacity() const { return p0.capacity; }
    u32 Size    () const { return p0.size; }

};
void ReserveParticles( Solver* solver, u32 count )
{
    array::reserve( solver->p0, count );
    array::reserve( solver->p1, count );
    array::reserve( solver->v , count );
    array::reserve( solver->w , count );
}

namespace
{
static PhysicsBody AllocateBody( Solver* solver, u32 particleAmount )
{
    if( solver->Size() + particleAmount > solver->Capacity() )
        return{};

    PhysicsBody body;
    body.begin = solver->Size();
    body.count = particleAmount;

    for( u32 i = 0; i < particleAmount; ++i )
    {
        array::push_back( solver->p0, Vector3F( 0.f ) );
        array::push_back( solver->p1, Vector3F( 0.f ) );
        array::push_back( solver->v, Vector3F( 0.f ) );
        array::push_back( solver->w, 1.f );
    }

    return body;
}

static inline bool IsValid( const Solver* solver, BodyIdInternal idi )
{
    return id_table::has( solver->id_tbl[idi.type], idi );
}
inline const PhysicsBody& GetPhysicsBody( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->bodies[idi.type][idi.index];
}
inline const PhysicsParams& GetPhysicsParams( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->params[idi.type][idi.index];
}
}//


void Create( Solver** solver, u32 maxParticles )
{
    Solver* s = BX_NEW( bxDefaultAllocator(), Solver );
    ReserveParticles( s, maxParticles );

    solver[0] = s;
}

void Destroy( Solver** solver )
{
    BX_DELETE0( bxDefaultAllocator(), solver[0] );
}

void SetFrequency( Solver* solver, u32 freq )
{
    SYS_ASSERT( freq != 0 );
    solver->frequency = freq;
    solver->delta_time = (f32)( 1.0 / (double)freq );
}

namespace
{
static void GarbageCollector( Solver* solver )
{
}

static void RebuldConstraints( Solver* solver )
{
    if( !solver->flags.rebuild_constraints )
        return;

    for( u32 iab = 0; iab < solver->active_bodies_count; ++iab )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[iab];
        
        if( idi.type == EBody::eSOFT )
        {
            const PhysicsSoftBody& soft = solver->soft_body[idi.index];

        }
        else if( idi.type == EBody::eCLOTH )
        {
        
        }
        else if( idi.type == EBody::eROPE )
        {
           
        }
    }


    solver->flags.rebuild_constraints = 0;
}

static void PredictPositions( Solver* solver, const PhysicsBody& body, const PhysicsParams& params, const Vector3F& gravityAcc, float deltaTime )
{
    const u32 pbegin = body.begin;
    const u32 pend = body.begin + body.count;

    SYS_ASSERT( pend <= solver->Size() );

    const float damping_coeff = ::pow( 1.f - params.vel_damping, deltaTime );

    const Vector3F gravityDV = gravityAcc * deltaTime;

    for( u32 i = pbegin; i < pend; ++i )
    {
        Vector3F p = solver->p0[i];
        Vector3F v = solver->v[i];

        v += gravityDV;
        v *= damping_coeff;

        p += v * deltaTime;

        solver->p1[i] = p;
        solver->v[i] = v;
    }
}
static void UpdateVelocities( Solver* solver, float deltaTime )
{
    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;

    const u32 pbegin = 0;
    const u32 pend = solver->Size();
    for( u32 i = pbegin; i < pend; ++i )
    {
        const Vector3F& p0 = solver->p0[i];
        const Vector3F& p1 = solver->p1[i];

        solver->v[i] = ( p1 - p0 ) * deltaTime;
        solver->p0[i] = p1;
    }
}


}//

void Solve( Solver* solver, u32 numIterations, float deltaTime )
{
    GarbageCollector( solver );
    RebuldConstraints( solver );

    const Vector3F gravity_acc( 0.f, -9.82f, 0.f );

    const u32 n = solver->active_bodies_count;
    for( u32 i = 0; i < n; ++i )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[i];
        const PhysicsBody& body = GetPhysicsBody( solver, idi );
        const PhysicsParams& params = GetPhysicsParams( solver, idi );

        PredictPositions( solver, body, params, gravity_acc, deltaTime );
    }

    // collision detection
    {}

    // solve constraints
    for( u32 sit = 0; sit < numIterations; ++sit )
    {
        // TODO: arrange constraints in linear sorted array with buckets to process constraints in effective way (even on gpu)
    }

    UpdateVelocities( solver, deltaTime );

}

namespace
{
    static BodyIdInternal CreateBody( Solver* solver, u32 numParticles, EBody::E type )
    {
        SYS_ASSERT( type < EBody::_COUNT_ );

        PhysicsBody body = AllocateBody( solver, numParticles );
        if( !IsValid( body ) )
            return{ 0 };
        
        BodyIdInternal idi = id_table::create( solver->id_tbl[type] );
        idi.type = type;
        
        solver->bodies[idi.type][idi.index] = body;

        const u32 index = solver->active_bodies_count++;
        solver->active_bodies_idi[index] = idi;
        
        return idi;
    }
}
BodyId CreateSoftBody( Solver* solver, u32 numParticles )
{
    BodyIdInternal idi = CreateBody( solver, numParticles, EBody::eSOFT );
    return ToBodyId( idi );
}

BodyId CreateCloth( Solver* solver, u32 numParticles )
{
    BodyIdInternal idi = CreateBody( solver, numParticles, EBody::eCLOTH );
    return ToBodyId( idi );
}

BodyId CreateRope( Solver* solver, u32 numParticles )
{
    BodyIdInternal idi = CreateBody( solver, numParticles, EBody::eROPE );
    return ToBodyId( idi );
}

void DestroyBody( Solver* solver, BodyId id )
{
    BodyIdInternal idi = ToBodyIdInternal( id );
    SYS_ASSERT( idi.type < EBody::_COUNT_ );

    if( !id_table::has( solver->id_tbl[idi.type], idi ) )
        return;

    const PhysicsBody& body = GetPhysicsBody( solver, idi );
    SYS_ASSERT( IsValid( body ) );

    id_table::destroy( solver->id_tbl[idi.type], idi );

    
    array::push_back( solver->_to_deallocate, body );

    solver->flags.rebuild_constraints = 1;
}


namespace
{
    static inline bool IsInRange( const PhysicsBody& body, u32 index )
    {
        return ( index >= body.begin ) && ( index < ( body.begin + body.count ) );
    }
    void ComputeConstraint( DistanceC* out, const Vector3F* positions, u32 i0, u32 i1 )
    {
        SYS_ASSERT( i0 < 0xFFFF );
        SYS_ASSERT( i1 < 0xFFFF );

        const Vector3F& p0 = positions[i0];
        const Vector3F& p1 = positions[i1];

        out->i0 = i0;
        out->i1 = i1;
        out->rl = length( p1 - p0 );
    }

    static void SetConstraints( PhysicsSoftBody* soft, const Solver* solver, const PhysicsBody& body, const ConstraintInfo* constraints, u32 numConstraints )
    {

    }
    static void SetConstraints( PhysicsClothBody* cloth, const Solver* solver, const PhysicsBody& body, const ConstraintInfo* constraints, u32 numConstraints )
    {

    }

    static void SetConstraints( PhysicsBodyRope* rope, const Solver* solver, const PhysicsBody& body, const ConstraintInfo* constraints, u32 numConstraints )
    {
        for( u32 i = 0; i < numConstraints; ++i )
        {
            const ConstraintInfo& info = constraints[i];
            if( info.type != ConstraintInfo::eDISTANCE )
                continue;

            const u32 i0 = info.index[0];
            const u32 i1 = info.index[1];

            if( IsInRange( body, i0 ) && IsInRange( body, i1 ) )
            {
                DistanceC c;
                ComputeConstraint( &c, solver->p0.begin(), i0, i1 );
                array::push_back( rope->distance_c, c );
            }
        }
    }
}//



void SetConstraints( Solver* solver, BodyId id, const ConstraintInfo* constraints, u32 numConstraints )
{
    BodyIdInternal idi = ToBodyIdInternal( id );
    if( !IsValid( solver, idi ) )
        return;

    const PhysicsBody& body = GetPhysicsBody( solver, idi );

    switch( idi.type )
    {
    case EBody::eSOFT:
        SetConstraints( &solver->soft_body[idi.index], solver, body, constraints, numConstraints );
        break;
    case EBody::eCLOTH:
        SetConstraints( &solver->cloth_body[idi.index], solver, body, constraints, numConstraints );
        break;
    case EBody::eROPE:
        SetConstraints( &solver->rope_body[idi.index], solver, body, constraints, numConstraints );
        break;
    default:
        SYS_ASSERT( false );
        break;
    }

    solver->flags.rebuild_constraints = 1;

}

}//
}}//