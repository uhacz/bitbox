#pragma once

#include <util/vectormath/vectormath.h>
#include "puzzle_physics_type.h"

namespace bx { namespace puzzle {

namespace physics
{


struct BodyParams
{
    f32 vel_damping = 0.1f;
    f32 static_friction = 0.7f;
    f32 dynamic_friction = 0.6f;
    f32 restitution = 0.2f;
    f32 stiffness = 1.0f;
};

struct BodyCoM // center of mass
{
    QuatF rot = QuatF::identity();
    Vector3F pos = Vector3F( 0.f );
};

struct DistanceCInfo
{
    u32 i0, i1;
};
struct ShapeMatchingCInfo
{
    Vector3F rest_pos{ 0.f }; // relative position (object space)
    Vector4F local_normal{ 0.f }; // used for sdf collision resolving
    u32 i = UINT32_MAX; // particle index
    f32 mass = 1.f;
};

// ---
void Create      ( Solver** solver, u32 maxParticles, float particleRadius = 0.1f );
void Destroy     ( Solver** solver );
void SetFrequency( Solver* solver, u32 freq );
f32  GetFrequency( Solver* solver );
void Solve       ( Solver* solver, u32 numIterations, float deltaTime );

// --- 
BodyId CreateBody ( Solver* solver, u32 numParticles );
void   DestroyBody( Solver* solver, BodyId id );
bool   IsBodyAlive( Solver* solver, BodyId id );
// ---
void     SetDistanceConstraints ( Solver* solver, BodyId id, const DistanceCInfo* constraints, u32 numConstraints, float stiffness = 1.f );
void     CalculateLocalPositions( Solver* solver, BodyId id, float stiffness = 1.f );
void     SetSDFData             ( Solver* solver, BodyId id, const Vector4F* sdfData, u32 count );




// --- 
u32       GetNbParticles          ( Solver* solver, BodyId id );
Vector3F* MapInterpolatedPositions( Solver* solver, BodyId id );
Vector3F* MapPosition             ( Solver* solver, BodyId id );
Vector3F* MapVelocity             ( Solver* solver, BodyId id );
f32*      MapMassInv              ( Solver* solver, BodyId id );
void      Unmap                   ( Solver* solver, void* ptr );

float     GetParticleRadius( const Solver* solver );
bool      GetBodyParams( BodyParams* params, const Solver* solver, BodyId id );
void      SetBodyParams( Solver* solver, BodyId id, const BodyParams& params );
void      SetExternalForce( Solver* solver, BodyId id, const Vector3F& force );
void      AddExternalForce( Solver* solver, BodyId id, const Vector3F& force );
void      SetBodySelfCollisions( Solver* solver, BodyId id, bool value );
BodyCoM   GetBodyCoM( Solver* solver, BodyId id );
BodyCoM   GetBodyCoMDisplacement( Solver* solver, BodyId id );

}//
}}//

namespace bx {namespace puzzle {
namespace physics
{

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


