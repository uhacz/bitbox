#pragma once

#include <util/vectormath/vectormath.h>
#include <util/bbox.h>

#include "puzzle_physics_type.h"

namespace bx { namespace puzzle {

namespace physics
{


struct FrictionParams
{
    f32 static_value = 0.1f;
    f32 dynamic_value = 0.1f;
    FrictionParams() {}
    FrictionParams( f32 s, f32 d )
        : static_value( s )
        , dynamic_value( d )
    {}
};

struct BodyCoM // center of mass
{
    QuatF rot = QuatF::identity();
    Vector3F pos = Vector3F( 0.f );
};
using BodyAABB = AABBF;


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
void  Create           ( Solver** solver, u32 maxParticles, float particleRadius = 0.1f );
void  Destroy          ( Solver** solver );
void  SetFrequency     ( Solver* solver, u32 freq );
f32   GetFrequency     ( Solver* solver );
float GetParticleRadius( const Solver* solver );
void  Solve            ( Solver* solver, u32 numIterations, float deltaTime );

// --- 
BodyId      CreateBody ( Solver* solver, u32 numParticles, const char* name = nullptr );
void        DestroyBody( Solver* solver, BodyId id );
bool        IsBodyAlive( Solver* solver, BodyId id );
void        SetName    ( Solver* solver, BodyId id, const char* name );
const char* GetName    ( Solver* solver, BodyId id );
BodyAABB    GetAABB    ( Solver* solver, BodyId id );

u32         GetNbBodies( Solver* solver );
BodyId      GetBodyId  ( Solver* solver, u32 index );

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


//bool      GetBodyParams( BodyParams* params, const Solver* solver, BodyId id );
//void      SetBodyParams( Solver* solver, BodyId id, const BodyParams& params );
FrictionParams GetFriction       ( Solver* solver, BodyId id );
float          GetRestitution    ( Solver* solver, BodyId id );
float          GetVelocityDamping( Solver* solver, BodyId id );
bool           SetFriction       ( Solver* solver, BodyId id, const FrictionParams& params );
bool           SetRestitution    ( Solver* solver, BodyId id, float value );
bool           SetVelocityDamping( Solver* solver, BodyId id, float value );


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
void DebugDraw( Solver* solver );

}//

}}//


