#pragma once


#include "../renderer_type.h"
#include "../renderer_camera.h"
#include "puzzle_physics.h"

// --- gfx
namespace bx { namespace puzzle {
namespace physics
{
struct Gfx;

void Create( Gfx** gfx, Solver* solver, gfx::Scene scene );
void Destroy( Gfx** gfx );

bool AddBody( Gfx* gfx, BodyId id );
void SetColor( Gfx* gfx, BodyId id, u32 colorRGBA );
void Tick( Gfx* gfx, rdi::CommandQueue* cmdq, const gfx::Camera& camera, const Matrix4& lightWorld, const Matrix4& lightProj );

}//
}}//
