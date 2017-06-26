#include "puzzle_physics.h"
#include "puzzle_physics_internal.h"
#include "../spatial_hash_grid.h"

#include <util/array.h>
#include <util/id_table.h>
#include <util/math.h>

#include <rdi/rdi_debug_draw.h>
#include "puzzle_physics_pbd.h"

namespace bx { namespace puzzle {

namespace physics
{





// --- bodies
struct Body
{
    u32 begin = 0;
    u32 count = 0;
};
inline bool IsValid( const Body& body ) { return body.count != 0; }
struct BodyCOM // center of mass
{
    QuatF rot = QuatF::identity();
    Vector3F pos = Vector3F( 0.f );
};

// --- body id
union BodyIdInternal
{
    u32 i;
    struct
    {
        u32 id    : 16;
        u32 index : 16;
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


using IdTable             = id_table_t< EConst::MAX_BODIES, BodyIdInternal >;
using Vector3Array        = array_t<Vector3F>;
using Vector4Array        = array_t<Vector4F>;
using F32Array            = array_t<f32>;
using U32Array            = array_t<u32>;
using U16Array            = array_t<u16>;

using PhysicsBodyArray        = array_t<Body>;
using BodyIdInternalArray     = array_t<BodyIdInternal>;
using CollisionCArray         = array_t<PlaneCollisionC>;
using ParticleCollisionCArray = array_t<ParticleCollisionC>;
using SDFCollisionCArray      = array_t<SDFCollisionC>;
using DistanceCArray          = array_t<DistanceC>;
using BendingCArray           = array_t<BendingC>;
using ShapeMatchingCArray     = array_t<ShapeMatchingC>;


// --- solver
struct Solver
{
    Vector3Array x; // interpolated positions
    Vector3Array pp;// prev positions (stored before each time step)
    Vector3Array p0;
    Vector3Array p1;
    Vector3Array v;
    F32Array     w;
    U16Array     body_index;
    F32Array     collision_r;
        
    IdTable    id_tbl;
    Body       bodies     [EConst::MAX_BODIES];
    BodyParams body_params[EConst::MAX_BODIES];
    BodyCOM    body_com   [EConst::MAX_BODIES];

    // constraints where points indices are absolute
    CollisionCArray         plane_collision_c;
    ParticleCollisionCArray particle_collision_c;
    SDFCollisionCArray      sdf_collision_c;
    
    // constraints where points indices are relative to body
    DistanceCArray      distance_c[EConst::MAX_BODIES];
    ShapeMatchingCArray shape_matching_c[EConst::MAX_BODIES];
    Vector4Array        sdf_normal[EConst::MAX_BODIES];
    
    BodyIdInternal   active_bodies_idi[EConst::MAX_BODIES] = {};
    u16              active_bodies_count = 0;

    BodyIdInternalArray _to_deallocate;

    HashGridStatic _hash_grid;

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
    array::reserve( solver->x , count );
    array::reserve( solver->pp, count );
    array::reserve( solver->p0, count );
    array::reserve( solver->p1, count );
    array::reserve( solver->v , count );
    array::reserve( solver->w , count );
    array::reserve( solver->body_index, count );
    array::reserve( solver->collision_r, count );
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
        array::push_back( solver->body_index, UINT16_MAX );
        array::push_back( solver->collision_r, 1.f );
    }

    return body;
}

static inline bool IsValid( const Solver* solver, BodyIdInternal idi )
{
    return id_table::has( solver->id_tbl, idi );
}
static inline const Body& GetBody( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->bodies[idi.index];
}
static inline const BodyParams& GetBodyParams( const Solver* solver, BodyIdInternal idi )
{
    SYS_ASSERT( IsValid( solver, idi ) );
    return solver->body_params[idi.index];
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
        float w = solver->w[i];
        w = ( w > FLT_EPSILON ) ? 1.f : 0.f;

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

        Vector3F v = ( p1 - p0 ) * delta_time_inv;
        solver->p0[i] = p1;

        const float restitution = solver->collision_r[i];
        v *= restitution;
        solver->collision_r[i] = 1.f;

        solver->v[i] = v;
    }
}
static void InterpolatePositions( Solver* solver )
{
    const float t = solver->delta_time_acc / solver->delta_time;

    const u32 pbegin = 0;
    const u32 pend = solver->Size();
    for( u32 i = pbegin; i < pend; ++i )
    {
        const Vector3F& pp = solver->pp[i];
        const Vector3F& p0 = solver->p0[i];

        solver->x[i] = lerp( t, pp, p0 );
    }
}

static void GenerateCollisionConstraints( Solver* solver )
{
    const float pradius2 = solver->particle_radius*2.f;
    const float pradius2_sqr = pradius2*pradius2;

    const Vector3F* points = solver->p1.begin();
    const u32 n = solver->p1.size;
    const u32 hash_grid_size = n * 4;

    array::reserve( solver->plane_collision_c, n * 2 );
    array::reserve( solver->particle_collision_c, n * 2 );
    array::reserve( solver->sdf_collision_c, n * 2 );
    array::clear( solver->plane_collision_c );
    array::clear( solver->particle_collision_c );
    array::clear( solver->sdf_collision_c );

    Build( &solver->_hash_grid, nullptr, points, n, hash_grid_size, pradius2 );

    const float collision_threshold = pradius2_sqr - FLT_EPSILON;
    const u32 n_active = solver->active_bodies_count;
    for( u32 iactive = 0; iactive < n_active; ++iactive )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[iactive];
        const u32 i = idi.index;

        const Body& body = solver->bodies[i];
        const u32 body_end = body.begin + body.count;

        for( u32 ip0 = body.begin; ip0 < body_end; ++ip0 )
        {
            const Vector3F& p0 = solver->p1[ip0];
            const i32x3 p0_grid = solver->_hash_grid.ComputeGridPos( p0 );
            for( i32 dz = -1; dz <= 1; ++dz )
            {
                for( i32 dy = -1; dy <= 1; ++dy )
                {
                    for( i32 dx = -1; dx <= 1; ++dx )
                    {
                        const i32x3 lookup_pos_grid( p0_grid.x + dx, p0_grid.y + dy, p0_grid.z + dz );
                        const HashGridStatic::Indices indices = solver->_hash_grid.Lookup( lookup_pos_grid );
                        for( u32 ip1 : indices )
                        {
                            if( ip1 == ip0 )
                                continue;

                            const float w0 = solver->w[ip0];
                            const float w1 = solver->w[ip1];
                            const float wsum = w0 + w1;
                            if( wsum < FLT_EPSILON )
                                continue;
                            
                            const Vector3F& p1 = solver->p1[ip1];
                            const Vector3F v = p1 - p0;
                            const float len_sqr = lengthSqr( v );
                            if( len_sqr >= collision_threshold )
                                continue;

                            const u32 body_i0 = solver->body_index[ip0];
                            const u32 body_i1 = solver->body_index[ip1];
                            const bool has_sdf0 = solver->sdf_normal[body_i0].size > 0;
                            const bool has_sdf1 = solver->sdf_normal[body_i1].size > 0;

                            if( has_sdf0 && has_sdf1 )
                            {
                                if( body_i0 != body_i1 )
                                {
                                    const Body& body0 = solver->bodies[body_i0];
                                    const Body& body1 = solver->bodies[body_i1];
                                    SYS_ASSERT( ip0 >= body0.begin );
                                    SYS_ASSERT( ip1 >= body1.begin );

                                    const u32 ip0_rel = ip0 - body0.begin;
                                    const u32 ip1_rel = ip1 - body1.begin;

                                    SYS_ASSERT( ip0_rel < solver->sdf_normal[body_i0].size );
                                    SYS_ASSERT( ip1_rel < solver->sdf_normal[body_i1].size );

                                    const Vector4F& sdf0 = solver->sdf_normal[body_i0][ip0_rel];
                                    const Vector4F& sdf1 = solver->sdf_normal[body_i1][ip1_rel];

                                    SDFCollisionC c;
                                    c.n = ( sdf0.w < sdf1.w ) ? sdf0.getXYZ() : -sdf1.getXYZ();
                                    c.d = minOfPair( sdf0.w, sdf1.w );
                                    c.i0 = ip0;
                                    c.i1 = ip1;
                                    array::push_back( solver->sdf_collision_c, c );
                                }

                            }
                            else
                            {
                                ParticleCollisionC c;
                                c.i0 = ip0;
                                c.i1 = ip1;
                                array::push_back( solver->particle_collision_c, c );
                            }
                        }// ip1
                    }// dx
                }// dy
            }// dz
        
            // --temp for testing
            PlaneCollisionC c;
            c.i = ip0;
            c.plane = makePlane( Vector3F::yAxis(), Vector3F( 0.f ) );
            array::push_back( solver->plane_collision_c, c );
        }// ip0
    }// iactive
}


static inline void ComputeFriction( Vector3F frictionDpos[2], Solver* solver, u32 i0, u32 i1, const Vector3F& newp0, const Vector3F& newp1, const Vector3F& n, float depth )
{
    const Vector3F& oldp0 = solver->p0[i0];
    const Vector3F& oldp1 = solver->p1[i1];

    const u16 body_index0 = solver->body_index[i0];
    const u16 body_index1 = solver->body_index[i1];
    const BodyParams& params0 = solver->body_params[body_index0];
    const BodyParams& params1 = solver->body_params[body_index1];

    const Vector3F xt = projectVectorOnPlane( ( newp1 - oldp1 ) - ( newp0 - oldp0 ), n );

    frictionDpos[0] = ComputeFrictionDeltaPos( xt, n, -depth, params0.static_friction, params0.dynamic_friction );
    frictionDpos[1] = ComputeFrictionDeltaPos( -xt, n, -depth, params1.static_friction, params1.dynamic_friction );
}
static void SolveCollisionConstraints( Solver* solver )
{
    const float pradius = solver->particle_radius;
    const float pradius2 = solver->particle_radius*2.f;
    const float pradius2_sqr = pradius2*pradius2;
    // collision
    for( const ParticleCollisionC& c : solver->particle_collision_c )
    {
        const Vector3F& p0 = solver->p1[c.i0];
        const Vector3F& p1 = solver->p1[c.i1];

        const Vector3F v = p1 - p0;
        const float dsqr = lengthSqr( v );
        if( dsqr < pradius2_sqr )
        {
            const float w0 = solver->w[c.i0];
            const float w1 = solver->w[c.i1];
            const float wsum = w0 + w1;
            const float wsum_inv = 1.f / wsum;
            const float d = ::sqrtf( dsqr );
            const float drcp = ( d > FLT_EPSILON ) ? 1.f / d : 0.f;
            const float depth = d - pradius2;
            const Vector3F n = v * drcp;
            const Vector3F dpos = n *  depth * wsum_inv;
            const Vector3F dpos0 = dpos * w0;
            const Vector3F dpos1 = -dpos * w1;

            const Vector3F newp0 = p0 + dpos0;
            const Vector3F newp1 = p1 + dpos1;
            
            Vector3F dpos_friction[2];
            ComputeFriction( dpos_friction, solver, c.i0, c.i1, newp0, newp1, n, depth );

            solver->p1[c.i0] = newp0 + dpos_friction[0]*w0 * wsum_inv;
            solver->p1[c.i1] = newp1 + dpos_friction[1]*w1 * wsum_inv;
        }
    }

    for( const SDFCollisionC& c : solver->sdf_collision_c )
    {
        const Vector3F& p0 = solver->p1[c.i0];
        const Vector3F& p1 = solver->p1[c.i1];

        const Vector3F v = p1 - p0;
        const float dsqr = lengthSqr( v );
        if( dsqr < pradius2_sqr )
        {
            const float w0 = solver->w[c.i0];
            const float w1 = solver->w[c.i1];
            const float wsum = w0 + w1;
            const float wsum_inv = 1.f / wsum;

            const float d = ::sqrtf( dsqr );
            
            Vector3F n = c.n;
            float depth = c.d;

            // boundary particle
            // modify contact normal to prevent bouncing
            if( c.d < d )
            {
                const float drcp = ( d > FLT_EPSILON ) ? 1.f / d : 0.f;
                n = v * drcp;
                const float v_dot_n = dot( v, c.n );
                if( v_dot_n < 0.f )
                    n = normalizeSafeF( n - c.n*v_dot_n*2.f );

                depth = d - pradius2;
            }

            const Vector3F dpos = n * depth * wsum_inv;
            const Vector3F dpos0 = dpos * w0;
            const Vector3F dpos1 =-dpos * w1;

            const Vector3F newp0 = p0 + dpos0;
            const Vector3F newp1 = p1 + dpos1;

            Vector3F dpos_friction[2];
            ComputeFriction( dpos_friction, solver, c.i0, c.i1, newp0, newp1, n, depth );

            solver->p1[c.i0] = newp0 + dpos_friction[0]*w0 * wsum_inv;
            solver->p1[c.i1] = newp1 + dpos_friction[1]*w1 * wsum_inv;
            
        }
    }

    for( const PlaneCollisionC& c : solver->plane_collision_c )
    {
        const Vector3F& p = solver->p1[c.i];
        float d = dot( c.plane, Vector4F( p, 1.f ) );
        if( d < pradius )
        {
            d -= pradius;
            const Vector3F n = c.plane.getXYZ();
            const Vector3F newp = p - (n * d);
            const Vector3F& oldp = solver->p0[c.i];

            const u16 body_index = solver->body_index[c.i];
            const BodyParams& params0 = solver->body_params[body_index];

            const Vector3F xt = projectVectorOnPlane( newp - oldp, n );
            const Vector3F dpos_friction = ComputeFrictionDeltaPos( xt, n, -d, params0.static_friction, params0.dynamic_friction );
            solver->p1[c.i] = newp - dpos_friction;
        }
    }
}


static void SolveDistanceConstraints( Solver* solver )
{
    const u32 n_active = solver->active_bodies_count;
    for( u32 iactive = 0; iactive < n_active; ++iactive )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[iactive];
        const u32 i = idi.index;

        const Body& body = solver->bodies[i];
        const BodyParams& params = solver->body_params[i];

        const DistanceCArray& carray = solver->distance_c[i];
        for( DistanceC c : carray )
        {
            const u32 i0 = body.begin + c.i0;
            const u32 i1 = body.begin + c.i1;

            const Vector3F& p0 = solver->p1[i0];
            const Vector3F& p1 = solver->p1[i1];
            const f32 w0 = solver->w[i0];
            const f32 w1 = solver->w[i1];

            Vector3F dpos0, dpos1;
            if( SolveDistanceC( &dpos0, &dpos1, p0, p1, w0, w1, c.rl, params.stiffness ) )
            {
                solver->p1[i0] += dpos0;
                solver->p1[i1] += dpos1;
            }
        }
    }
}

static void SolveShapeMatchingConstraints( Solver* solver )
{
    const u32 n_active = solver->active_bodies_count;
    for( u32 iactive = 0; iactive < n_active; ++iactive )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[iactive];
        const u32 i = idi.index;
        const ShapeMatchingCArray& shape_matching_c = solver->shape_matching_c[i];
        if( array::empty( shape_matching_c ) )
            continue;

        const Body&     body = solver->bodies[i];
        const Vector3F* pos = solver->p1.begin() + body.begin;
        SYS_ASSERT( shape_matching_c.size == body.count );


        Matrix3F rotation;
        Vector3F center_of_mass_pos;
        SoftBodyUpdatePose( &rotation, &center_of_mass_pos, pos, shape_matching_c.begin(), body.count );

        BodyCOM& com = solver->body_com[i];
        com.rot = normalize( QuatF( rotation ) );
        com.pos = center_of_mass_pos;

        for( u32 i = 0; i < body.count; ++i )
        {
            const u32 pindex = body.begin + i;
            const Vector3F& p = solver->p1[pindex];
            const ShapeMatchingC& c = shape_matching_c[i];

            Vector3F dpos;
            SolveShapeMatchingC( &dpos, rotation, center_of_mass_pos, c.rest_pos, p, 1.f );
            solver->p1[pindex] += dpos;
        }

    }
}


static void SolveInternal( Solver* solver, u32 numIterations )
{
    const float deltaTime = solver->delta_time;
    const float pradius = solver->particle_radius;
    const float pradius2 = 2 * pradius;
    const float pradius2_sqr = pradius2*pradius2;

    const Vector3F gravity_acc( 0.f, -9.82f, 0.f );

    const u32 n_active = solver->active_bodies_count;
    for( u32 i = 0; i < n_active; ++i )
    {
        const BodyIdInternal idi = solver->active_bodies_idi[i];
        const Body& body = GetBody( solver, idi );
        const BodyParams& params = GetBodyParams( solver, idi );

        PredictPositions( solver, body, params, gravity_acc, deltaTime );
    }

    // collision detection
    {
        GenerateCollisionConstraints( solver );
    }

    // solve constraints
    for( u32 sit = 0; sit < numIterations; ++sit )
    {
        SolveDistanceConstraints( solver );
        SolveShapeMatchingConstraints( solver );        
        SolveCollisionConstraints( solver );
    }

    UpdateVelocities( solver, deltaTime );
}

}//

void Solve( Solver* solver, u32 numIterations, float deltaTime )
{
    GarbageCollector( solver );

    //solver->delta_time = 0.001f;
    solver->delta_time_acc += deltaTime;
    solver->delta_time_acc = clamp( solver->delta_time_acc, 0.f, 0.5f );
    while( solver->delta_time_acc >= solver->delta_time )
    {
        memcpy( solver->pp.begin(), solver->p0.begin(), solver->Size() * sizeof( Vector3F ) );
        SolveInternal( solver, numIterations );
        solver->delta_time_acc -= solver->delta_time;
    }

    InterpolatePositions( solver );

}

namespace
{
    static BodyIdInternal CreateBodyInternal( Solver* solver, u32 numParticles )
    {
        Body body = AllocateBody( solver, numParticles );
        if( !IsValid( body ) )
            return{ 0 };
        
        BodyIdInternal idi = id_table::create( solver->id_tbl );
        
        solver->bodies[idi.index] = body;
        for( u32 i = body.begin; i < body.begin + numParticles; ++i )
            solver->body_index[i] = idi.index;

        const u32 index = solver->active_bodies_count++;
        solver->active_bodies_idi[index] = idi;
        
        return idi;
    }
}
BodyId CreateBody( Solver* solver, u32 numParticles )
{
    BodyIdInternal idi = CreateBodyInternal( solver, numParticles );
    return ToBodyId( idi );
}

void DestroyBody( Solver* solver, BodyId id )
{
    BodyIdInternal idi = ToBodyIdInternal( id );

    if( !id_table::has( solver->id_tbl, idi ) )
        return;

    const Body& body = GetBody( solver, idi );
    SYS_ASSERT( IsValid( body ) );

    id_table::destroy( solver->id_tbl, idi );
    array::push_back( solver->_to_deallocate, idi );
}

bool IsBodyAlive( Solver * solver, BodyId id )
{
    BodyIdInternal idi = ToBodyIdInternal( id );
    return id_table::has( solver->id_tbl, idi );
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
    

}//

void SetDistanceConstraints( Solver* solver, BodyId id, const DistanceCInfo* constraints, u32 numConstraints )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    const Body& body = GetBody( solver, idi );

    DistanceCArray& outArray = solver->distance_c[idi.index];

    for( u32 i = 0; i < numConstraints; ++i )
    {
        const DistanceCInfo& info = constraints[i];

        const u32 relative_i0 = info.i0;
        const u32 relative_i1 = info.i1;
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

void CalculateLocalPositions( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    const Body& body = GetBody( solver, idi );

    ShapeMatchingCArray& sm_out_array = solver->shape_matching_c[idi.index];
    array::clear( sm_out_array );
    array::reserve( sm_out_array, body.count );

    const u32 body_end = body.begin + body.count;
    const Vector3F* rest_positions = solver->p0.begin();
    const f32* mass_inv = solver->w.begin();

    // calculate mean
    Vector3F shape_offset( 0.f );

    for( u32 i = body.begin; i < body_end; ++i )
        shape_offset += rest_positions[i];

    shape_offset /= float( body.count );
    
    // calculate center of mass
    Vector3F com( 0.f );

    // By substracting shape_offset the calculation is done in relative coordinates
    for( u32 i = body.begin; i < body_end; ++i )
        com += rest_positions[i] -shape_offset;

    com /= float( body.count );

    for( u32 i = body.begin; i < body_end; ++i )
    {
        ShapeMatchingC c;
        c.mass = ( mass_inv[i] > FLT_EPSILON ) ? 1.f / mass_inv[i] : 0.f;
        c.rest_pos = (rest_positions[i] - shape_offset) - com;
        array::push_back( sm_out_array, c );
    }
}

void SetSDFData( Solver* solver, BodyId id, const Vector4F* sdfData, u32 count )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    const Body& body = GetBody( solver, idi );
    SYS_ASSERT( body.count == count );

    Vector4Array& sdf_out_array = solver->sdf_normal[idi.index];
    array::clear( sdf_out_array );
    array::reserve( sdf_out_array, body.count );

    for( u32 i = 0; i < count; ++i )
    {
        array::push_back( sdf_out_array, sdfData[i] );
    }
}

#if 0
void SetShapeMatchingConstraints( Solver* solver, BodyId id, const ShapeMatchingCInfo* constraints, u32 numConstraints )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    const Body& body = GetBody( solver, idi );

    ShapeMatchingCArray& sm_out_array = solver->shape_matching_c[idi.index];
    Vector4Array& sdf_out_array = solver->sdf_normal[idi.index];
    Vector3F* body_pos = MapPosition( solver, id );

    for( u32 i = 0; i < numConstraints; ++i )
    {
        const ShapeMatchingCInfo& info = constraints[i];

        const u32 relative_i = info.i;
        const u32 absolute_i = body.begin + relative_i;

        if( IsInRange( body, absolute_i ) )
        {
            ShapeMatchingC c;
            c.rest_pos = info.rest_pos;
            c.mass = info.mass;
            array::push_back( sm_out_array, c );
            array::push_back( sdf_out_array, info.local_normal );
        }
    }


}
#endif

u32 GetNbParticles( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( 0 );
    return GetBody( solver, ToBodyIdInternal( id ) ).count;
}

Vector3F* MapInterpolatedPositions( Solver* solver, BodyId id )
{
    PHYSICS_VALIDATE_ID( nullptr );
    Body body = GetBody( solver, ToBodyIdInternal( id ) );
    return solver->x.begin() + body.begin;
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
    params[0] = solver->body_params[idi.index];
    return true;
}

void SetBodyParams( Solver* solver, BodyId id, const BodyParams& params )
{
    PHYSICS_VALIDATE_ID_NO_RETURN;

    const BodyIdInternal idi = ToBodyIdInternal( id );
    solver->body_params[idi.index] = params;
}

float GetParticleRadius( const Solver* solver )
{
    return solver->particle_radius;
}

}//
}}//

namespace bx{ namespace puzzle{

namespace physics{



void DebugDraw( Solver* solver, BodyId id, const DebugDrawBodyParams& params )
{
    BodyIdInternal idi = ToBodyIdInternal( id );
    if( !IsValid( solver, idi ) )
        return;

    const Body& body = GetBody( solver, idi );
    const Vector3F* points = solver->x.begin() + body.begin;
    
    if( params.draw_points )
    {
        const float radius = solver->particle_radius;
        for( u32 i = 0; i < body.count; ++i )
            rdi::debug_draw::AddSphere( Vector4F( points[i], radius ), params.point_color, 1 );
    }
    if( params.draw_constraints )
    {
        {
            const DistanceCArray& constraints = solver->distance_c[idi.index];
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


