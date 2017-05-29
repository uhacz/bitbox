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
    namespace EConst
    {
        enum E
        {
            eMAX_BODIES = 64,
        };
    }//

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

    struct ParticleManager
    {
        u32 capacity;
        u32 size;


    };


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
    using DistanceCArray = array_t<DistanceC>;
    using BendingCArray  = array_t<BendingC>;

    // --- bodies
    struct PhysicsBody
    {
        u32 begin = 0;
        u32 count = 0;
    };
    inline bool IsAlive( const PhysicsBody& body ) { return body.count != 0; }

    struct PhysicsSoftBody
    {
        PhysicsBody body;
        u32 rest_pos_begin = 0;
        u32 rest_pos_count = 0;

        Vector3Array   rest_p;
        DistanceCArray distance_c;
    };
    struct PhysicsClothBody
    {
        PhysicsBody body;
        DistanceCArray distance_c;
    };
    struct PhysicsBodyRope
    {
        PhysicsBody body;
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


// --- solver
struct Solver
{
    Vector3Array p0;
    Vector3Array p1;
    Vector3Array v;
    Vector3Array w;
        
    IdTable          id_tbl    [EBody::_COUNT_];
    PhysicsSoftBody  soft_body [EConst::eMAX_BODIES];
    PhysicsClothBody cloth_body[EConst::eMAX_BODIES];

    u32 num_iterations = 4;
    u32 frequency = 60;
    f32 delta_time = 1.f / frequency;
};
void ReserveParticles( Solver* solver, u32 count )
{
    array::reserve( solver->p0, count );
    array::reserve( solver->p1, count );
    array::reserve( solver->v , count );
    array::reserve( solver->w , count );
}

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

void Solve( Solver* solver, u32 numIterations, float deltaTime )
{

}

BodyId CreateSoftBody( Solver* solver, u32 numParticles )
{

}

BodyId CreateCloth( Solver* solver, u32 numParticles )
{

}

BodyId CreateRope( Solver* solver, u32 numParticles )
{
    BodyIdInternal idi = id_table::create( solver->id_tbl[EBody::eROPE] );

}
void DestroyBody( Solver* solver, BodyId id )
{

}

}//
}}//