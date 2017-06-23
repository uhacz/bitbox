#pragma once

#include <util/vectormath/vectormath.h>

namespace bx { namespace puzzle {

namespace physics
{
struct Solver;
struct BodyId { u32 i; };
inline BodyId BodyIdInvalid() { return { 0 }; }
static inline bool operator == ( const BodyId a, const BodyId b ) { return a.i == b.i; }

struct BodyParams
{
    f32 vel_damping = 0.1f;
    f32 static_friction = 0.7f;
    f32 dynamic_friction = 0.6f;
    f32 restitution = 0.2f;
    f32 stiffness = 1.0f;
};

struct DistanceCInfo
{
    u32 i0, i1;
};
struct ShapeMatchingCInfo
{
    Vector3F rest_pos{ 0.f }; // relative position (object space)
    u32 i = UINT32_MAX; // particle index
    f32 mass = 1.f;
};

// ---
void Create      ( Solver** solver, u32 maxParticles, float particleRadius = 0.1f );
void Destroy     ( Solver** solver );
void SetFrequency( Solver* solver, u32 freq );
void Solve       ( Solver* solver, u32 numIterations, float deltaTime );

// --- 
BodyId CreateBody ( Solver* solver, u32 numParticles );
void   DestroyBody( Solver* solver, BodyId id );
bool   IsBodyAlive( Solver* solver, BodyId id );
// ---
//void   SetConstraints( Solver* solver, BodyId id, const ConstraintInfo* constraints, u32 numConstraints );
void     SetDistanceConstraints( Solver* solver, BodyId id, const DistanceCInfo* constraints, u32 numConstraints );
void     SetShapeMatchingConstraints( Solver* solver, BodyId id, const ShapeMatchingCInfo* constraints, u32 numConstraints );

// --- 
u32       GetNbParticles  ( Solver* solver, BodyId id );
Vector3F* MapPosition     ( Solver* solver, BodyId id );
Vector3F* MapVelocity     ( Solver* solver, BodyId id );
f32*      MapMassInv      ( Solver* solver, BodyId id );
void      Unmap           ( Solver* solver, void* ptr );

bool      GetBodyParams( BodyParams* params, const Solver* solver, BodyId id );
void      SetBodyParams( Solver* solver, BodyId id, const BodyParams& params );
float     GetParticleRadius( const Solver* solver );

}//
}}//

// --- utils
namespace bx {
namespace puzzle {

namespace physics
{

BodyId CreateRope( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len, float particleMass );
BodyId CreateCloth( Solver* solver, const Vector3F& attach, const Vector3F& axis, float width, float height, float particleMass );
BodyId CreateSoftBox( Solver* solver, const Matrix4F& pose, float width, float depth, float height, float particleMass, bool shell = false );
BodyId CreateRigidBox( Solver* solver, const Matrix4F& pose, float width, float depth, float height, float particleMass );


//////////////////////////////////////////////////////////////////////////
struct DebugDrawBodyParams
{
    u32 point_color = 0xFFFFFFFF;
    u32 constraint_color = 0xFF0000FF;
    u16 draw_points = 1;
    u16 draw_constraints = 1;

    DebugDrawBodyParams& Points( u32 color )
    {
        point_color = color;
        draw_points = 1;
        return *this;
    }
    DebugDrawBodyParams& Constraints( u32 color )
    {
        constraint_color = color;
        draw_constraints = 1;
        return *this;
    }
    DebugDrawBodyParams& NoPoints() { draw_points = 0; return *this; }
    DebugDrawBodyParams& NoConstraints() { draw_constraints = 0; return *this; }
};
void DebugDraw( Solver* solver, BodyId id, const DebugDrawBodyParams& params );

}//

}}//


