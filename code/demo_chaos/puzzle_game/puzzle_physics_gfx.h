#pragma once


#include "../renderer_type.h"
#include "../renderer_camera.h"
#include "puzzle_physics.h"

// --- gfx
namespace bx { namespace puzzle {
namespace physics
{
void CreateGfx( Gfx** gfx, Solver* solver, gfx::Scene scene );
void DestroyGfx( Gfx** gfx );

u32  AddBody( Gfx* gfx, BodyId id );
u32  AddActor( Gfx* gfx, u32 numParticles, u32 colorRGBA = 0xFFFFFFFF );

u32  FindBody( Gfx* gfx, BodyId id );
void SetColor( Gfx* gfx, BodyId id, u32 colorRGBA );
void SetColor( Gfx* gfx, u32 idnex, u32 colorRGBA );
void SetParticleData( Gfx* gfx, rdi::CommandQueue* cmdq, u32 index, const Vector3F* pdata, u32 count );

void Tick( Gfx* gfx, rdi::CommandQueue* cmdq, const gfx::Camera& camera, const Matrix4& lightWorld, const Matrix4& lightProj );


}//
}}//
