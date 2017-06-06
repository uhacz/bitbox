#pragma once

#include <util/vectormath/vectormath.h>

namespace bx { namespace puzzle {

namespace physics
{
struct Solver;
struct BodyId { u32 i; };
inline BodyId BodyIdInvalid() { return { 0 }; }

struct BodyParams
{
    f32 vel_damping = 0.1f;
    f32 static_friction = 0.1f;
    f32 dynamic_friction = 0.1f;
    f32 restitution = 0.2f;
};

struct ConstraintInfo
{
    enum EType : u32
    {
        eDISTANCE = 0,
        _COUNT_,
    };

    EType type = eDISTANCE;
    u32 index[4] = {};
};

// ---
void Create      ( Solver** solver, u32 maxParticles, float particleRadius = 0.1f );
void Destroy     ( Solver** solver );
void SetFrequency( Solver* solver, u32 freq );
void Solve       ( Solver* solver, u32 numIterations, float deltaTime );

// --- 
BodyId CreateSoftBody( Solver* solver, u32 numParticles );
BodyId CreateCloth   ( Solver* solver, u32 numParticles );
BodyId CreateRope    ( Solver* solver, u32 numParticles );
void   DestroyBody   ( Solver* solver, BodyId id );

// ---
void   SetConstraints( Solver* solver, BodyId id, const ConstraintInfo* constraints, u32 numConstraints );

// --- 
u32       GetNbParticles  ( Solver* solver, BodyId id );
Vector3F* MapPosition     ( Solver* solver, BodyId id );
Vector3F* MapVelocity     ( Solver* solver, BodyId id );
f32*      MapMassInv      ( Solver* solver, BodyId id );
Vector3F* MapRestPosition ( Solver* solver, BodyId id );
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

BodyId CreateRopeAtPoint( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len );


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
