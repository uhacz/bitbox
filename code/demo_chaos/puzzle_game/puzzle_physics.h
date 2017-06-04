#pragma once

#include <util/vectormath/vectormath.h>

namespace bx { namespace puzzle {

namespace physics
{
struct Solver;
struct BodyId { u32 i; };
inline BodyId BodyIdInvalid() { return { 0 }; }


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
void Create      ( Solver** solver, u32 maxParticles );
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



}//
}}//