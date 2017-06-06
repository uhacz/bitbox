#include "puzzle_physics.h"
#include <util/array.h>
#include <util/id_table.h>
#include "rdi/rdi_debug_draw.h"

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

// --- bodies
struct Body
{
    u32 begin = 0;
    u32 count = 0;
};
inline bool IsValid( const Body& body ) { return body.count != 0; }

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
static inline bool operator == ( const BodyIdInternal a, const BodyIdInternal b ) { return a.i == b.i; }

#define _PHYSICS_VALIDATE_ID( returnValue ) \
    { \
        BodyIdInternal idi = ToBodyIdInternal( id ); \
        if( !IsValid( solver, idi ) ) \
            return returnValue; \
    }

#define PHYSICS_VALIDATE_ID( returnValue ) _PHYSICS_VALIDATE_ID( returnValue )
#define  PHYSICS_VALIDATE_ID_NO_RETURN _PHYSICS_VALIDATE_ID( ; )

static inline BodyId         ToBodyId        ( BodyIdInternal idi ) { return{ idi.i }; }
static inline BodyIdInternal ToBodyIdInternal( BodyId id )          { return{ id.i }; }
using IdTable = id_table_t< EConst::eMAX_BODIES, BodyIdInternal >;
using PhysicsBodyArray = array_t<Body>;
using BodyIdInternalArray = array_t<BodyIdInternal>;


// --- solver
struct Solver
{
    Vector3Array p0;
    Vector3Array p1;
    Vector3Array v;
    F32Array     w;
        
    IdTable    id_tbl     [EBody::_COUNT_];
    Body       bodies     [EBody::_COUNT_][EConst::eMAX_BODIES];
    BodyParams body_params[EBody::_COUNT_][EConst::eMAX_BODIES];

    // constraints points indices are relative to body
    DistanceCArray distance_c[EBody::_COUNT_][EConst::eMAX_BODIES];

    //PhysicsSoftBody  soft_body [EConst::eMAX_BODIES];
    //PhysicsClothBody cloth_body[EConst::eMAX_BODIES];
    //PhysicsBodyRope  rope_body [EConst::eMAX_BODIES];

    BodyIdInternal   active_bodies_idi[EConst::eMAX_OBJECTS] = {};
    u16              active_bodies_count = 0;

    BodyIdInternalArray _to_deallocate;

    u32 num_iterations = 4;
    u32 frequency = 60;
    f32 delta_time = 1.f / frequency;
    f32 delta_time_acc = 0.f;
    f32 particle_radius = 0.1f;

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
namespace
{

static void ShutDown( Solver* solver )
{
    
}
static void ReserveParticles( Solver* solver, u32 count )
{
    array::reserve( solver->p0, count );
    array::reserve( solver->p1, count );
    array::reserve( solver->v , count );
    array::reserve( solver->w , count );
}

static Body AllocateBody( Solver* solver, u32 particleAmount )
{
    if( solver->Size() + particleAmount > solver->Capacity() )
        return{};

    Body body;
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
static inline const Body& GetBody( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->bodies[idi.type][idi.index];
}
static inline const BodyParams& GetBodyParams( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->body_params[idi.type][idi.index];
}
}//


void Create( Solver** solver, u32 maxParticles, float particleRadius )
{
    Solver* s = BX_NEW( bxDefaultAllocator(), Solver );
    ReserveParticles( s, maxParticles );
    s->particle_radius = particleRadius;
    solver[0] = s;
}

void Destroy( Solver** solver )
{
    if( !solver[0] )
        return;

    ShutDown( solver[0] );
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
    for( BodyIdInternal idi : solver->_to_deallocate )
    {
        for( u32 iactive = 0; iactive < solver->active_bodies_count; ++iactive )
        {
            if( idi == solver->active_bodies_idi[iactive] )
            {
                const u32 last_index = --solver->active_bodies_count;
                solver->active_bodies_idi[iactive] = solver->active_bodies_idi[last_index];
                break;
            }
        }
    }
}

static void RebuldConstraints( Solver* solver )
{
}

static void PredictPositions( Solver* solver, const Body& body, const BodyParams& params, const Vector3F& gravityAcc, float deltaTime )
{
    const u32 pbegin = body.begin;
    const u32 pend = body.begin + body.count;

    SYS_ASSERT( pend <= solver->Size() );

    const float damping_coeff = ::powf( 1.f - params.vel_damping, deltaTime );

    const Vector3F gravityDV = gravityAcc * deltaTime;

    for( u32 i = pbegin; i < pend; ++i )
    {
        Vector3F p = solver->p0[i];
        Vector3F v = solver->v[i];
        const float w = solver->w[i];

        v += gravityDV * w;
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

        solver->v[i] = ( p1 - p0 ) * delta_time_inv;
        solver->p0[i] = p1;
    }
}

inline int SolveDistanceC(
    Vector3F* resultA, Vector3F* resultB,
    const Vector3F& posA, const Vector3F& posB,
    f32 massInvA, f32 massInvB,
    f32 restLength, f32 stiffness )
{
    float wSum = massInvA + massInvB;
    if( wSum < FLT_EPSILON )
        return 0;

    float wSumInv = 1.f / wSum;
    
    Vector3F n = posB - posA;
    float d = length( n );
    n = ( d > FLT_EPSILON ) ? n / d : n;

    const float diff = d - restLength;
    const Vector3F dpos = n * stiffness * diff * wSumInv;

    resultA[0] = dpos * massInvA;
    resultB[0] = -dpos * massInvB;

    return 1;
}

static void SolveInternal( Solver* solver, u32 numIterations )
{
    const float deltaTime = solver->delta_time;
    const Vector3F gravity_acc( 0.f, -9.82f, 0.f );

    const u32 n = solver->active_bodies_count;
    for( u32 i = 0; i < n; ++i )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[i];
        const Body& body = GetBody( solver, idi );
        const BodyParams& params = GetBodyParams( solver, idi );

        PredictPositions( solver, body, params, gravity_acc, deltaTime );
    }

    // collision detection
    {}

    // solve constraints
    for( u32 sit = 0; sit < numIterations; ++sit )
    {
        for( u32 itype = 0; itype < EBody::_COUNT_; ++itype )
        {
            for( u32 i = 0; i < EConst::eMAX_BODIES; ++i )
            {
                const Body& body = solver->bodies[itype][i];
                const DistanceCArray& carray = solver->distance_c[itype][i];
                for( DistanceC c : carray )
                {
                    const u32 i0 = body.begin + c.i0;
                    const u32 i1 = body.begin + c.i1;

                    const Vector3F& p0 = solver->p1[i0];
                    const Vector3F& p1 = solver->p1[i1];
                    const f32 w0 = solver->w[i0];
                    const f32 w1 = solver->w[i1];

                    Vector3F dpos0, dpos1;
                    if( SolveDistanceC( &dpos0, &dpos1, p0, p1, w0, w1, c.rl, 1.f ) )
                    {
                        solver->p1[i0] += dpos0;
                        solver->p1[i1] += dpos1;
                    }
                }
            }
        }
    }

    UpdateVelocities( solver, deltaTime );
}

}//

void Solve( Solver* solver, u32 numIterations, float deltaTime )
{
    GarbageCollector( solver );
    RebuldConstraints( solver );

    solver->delta_time_acc += deltaTime;

    while( solver->delta_time_acc >= solver->delta_time )
    {
        SolveInternal( solver, numIterations );
        solver->delta_time_acc -= solver->delta_time;
    }

}

namespace
{
    static BodyIdInternal CreateBody( Solver* solver, u32 numParticles, EBody::E type )
    {
        SYS_ASSERT( type < EBody::_COUNT_ );

        Body body = AllocateBody( solver, numParticles );
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

    const Body& body = GetBody( solver, idi );
    SYS_ASSERT( IsValid( body ) );

    id_table::destroy( solver->id_tbl[idi.type], idi );
    array::push_back( solver->_to_deallocate, idi );

    //solver->flags.rebuild_constraints = 1;
}


namespace
{
    static inline bool IsInRange( const Body& body, u32 index )
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

    //static void SetConstraints( PhysicsSoftBody* soft, const Solver* solver, const PhysicsBody& body, const ConstraintInfo* constraints, u32 numConstraints )
    //{

    //}
    //static void SetConstraints( PhysicsClothBody* cloth, const Solver* solver, const PhysicsBody& body, const ConstraintInfo* constraints, u32 numConstraints )
    //{

    //}

    static void SetConstraintsRope( BodyIdInternal idi, Solver* solver, const ConstraintInfo* constraints, u32 numConstraints )
    {
        const Body& body = GetBody( solver, idi );
        DistanceCArray& outArray = solver->distance_c[EBody::eROPE][idi.index];
        for( u32 i = 0; i < numConstraints; ++i )
        {
            const ConstraintInfo& info = constraints[i];
            if( info.type != ConstraintInfo::eDISTANCE )
                continue;

            const u32 relative_i0 = info.index[0];
            const u32 relative_i1 = info.index[1];
            const u32 absolute_i0 = body.begin + relative_i0;
            const u32 absolute_i1 = body.begin + relative_i1;

            if( IsInRange( body, absolute_i0 ) && IsInRange( body, absolute_i1 ) )
            {
                DistanceC c;
                ComputeConstraint( &c, solver->p0.begin() + body.begin, relative_i0, relative_i1 );
                array::push_back( outArray, c );
            }
        }
    }
}//



void SetConstraints( Solver* solver, BodyId id, const ConstraintInfo* constraints, u32 numConstraints )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;
    //BodyIdInternal idi = ToBodyIdInternal( id );
    //if( !IsValid( solver, idi ) )
    //    return;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    const Body& body = GetBody( solver, idi );

    switch( idi.type )
    {
    case EBody::eSOFT:
        //SetConstraints( &solver->soft_body[idi.index], solver, body, constraints, numConstraints );
        break;
    case EBody::eCLOTH:
        //SetConstraints( &solver->cloth_body[idi.index], solver, body, constraints, numConstraints );
        break;
    case EBody::eROPE:
        SetConstraintsRope( idi, solver, constraints, numConstraints );
        //SetConstraints( &solver->rope_body[idi.index], solver, body, constraints, numConstraints );
        break;
    default:
        SYS_ASSERT( false );
        break;
    }

    //solver->flags.rebuild_constraints = 1;

}

u32 GetNbParticles( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( 0 );
    return GetBody( solver, ToBodyIdInternal( id ) ).count;
}

Vector3F* MapPosition( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( nullptr );
    Body body = GetBody( solver, ToBodyIdInternal( id ) );
    return solver->p0.begin() + body.begin;
}

Vector3F* MapVelocity( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( nullptr );

    Body body = GetBody( solver, ToBodyIdInternal( id ) );
    return solver->v.begin() + body.begin;
}

f32* MapMassInv( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( nullptr );

    Body body = GetBody( solver, ToBodyIdInternal( id ) );
    return solver->w.begin() + body.begin;
}

Vector3F* MapRestPosition( Solver* solver, BodyId id )
{
    return nullptr;
}

void Unmap( Solver* solver, void* ptr )
{
    ( void )solver;
    // TODO: implement
}

bool GetBodyParams( BodyParams* params, const Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( false );

    const BodyIdInternal idi = ToBodyIdInternal( id );
    params[0] = solver->body_params[idi.type][idi.index];
    return true;
}

void SetBodyParams( Solver* solver, BodyId id, const BodyParams& params )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    solver->body_params[idi.type][idi.index] = params;
}

float GetParticleRadius( const Solver* solver )
{
    return solver->particle_radius;
}

}//
}}//

namespace bx{ namespace puzzle{

namespace physics{

BodyId CreateRopeAtPoint( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len )
{
    const float pradius = physics::GetParticleRadius( solver );

    const u32 num_points = (u32)( len / pradius );
    const float step = len / (float)( num_points - 1 );
    BodyId rope = physics::CreateRope( solver, num_points );

    Vector3F* points = physics::MapPosition( solver, rope );
    f32* mass_inv = physics::MapMassInv( solver, rope );

    points[0] = attach;
    mass_inv[0] = 0.f;
    for( u32 i = 1; i < num_points; ++i )
    {
        points[i] = points[i - 1] + axis * step;
        mass_inv[i] = 1.f;
    }

    physics::Unmap( solver, mass_inv );
    physics::Unmap( solver, points );

    array_t< physics::ConstraintInfo >c_info;
    array::reserve( c_info, num_points - 1 );
    for( u32 i = 0; i < num_points - 1; ++i )
    {
        physics::ConstraintInfo& info = c_info[i];
        info.type = physics::ConstraintInfo::eDISTANCE;
        info.index[0] = i;
        info.index[1] = i + 1;
    }
    physics::SetConstraints( solver, rope, c_info.begin(), num_points - 1 );

    return rope;
}

void DebugDraw( Solver* solver, BodyId id, const DebugDrawBodyParams& params )
{
    BodyIdInternal idi = ToBodyIdInternal( id );
    if( !IsValid( solver, idi ) )
        return;

    const Body& body = GetBody( solver, idi );
    const Vector3F* points = solver->p0.begin() + body.begin;

    if( params.draw_points )
    {
        const float radius = solver->particle_radius;
        for( u32 i = 0; i < body.count; ++i )
            rdi::debug_draw::AddSphere( Vector4F( points[i], radius ), params.point_color, 1 );
    }
    if( params.draw_constraints )
    {
        {
            const DistanceCArray& constraints = solver->distance_c[idi.type][idi.index];
            for( DistanceC c : constraints )
            {
                const Vector3F& p0 = points[c.i0];
                const Vector3F& p1 = points[c.i1];
                rdi::debug_draw::AddLine( p0, p1, params.constraint_color, 1 );
            }
        }
    }
}

}//
}}//