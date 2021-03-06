#pragma once

#include "puzzle_physics_type.h"
#include <util\vectormath\vectormath.h>
#include "../renderer_camera.h"


struct bxPolyShape;
namespace bx {namespace puzzle {
namespace physics
{

BodyId CreateRope( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len, float particleMass );
BodyId CreateCloth( Solver* solver, const Vector3F& attach, const Vector3F& axis, float width, float height, float particleMass );
//BodyId CreateSoftBox( Solver* solver, const Matrix4F& pose, float width, float depth, float height, float particleMass, bool shell = false );

BodyId CreateFromShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const Vector3F* srcPos, u32 numPositions, const u32* srcIndices, u32 numIndices, float particleMass, float spacingFactor = 2.f, float jitter = 0.005f );
//BodyId CreateFromPolyShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const bxPolyShape& shape, float particleMass, float spacingFactor, float jitter );

BodyId CreateBox( Solver* solver, const Matrix4F& pose, const Vector3F& extents, float particleMass );
BodyId CreateSphere( Solver* solver, const Matrix4F& pose, float radius, float particleMass, int subdiv = 1 );

}}}//

namespace bx {namespace puzzle {
namespace physics
{

struct GUIContext;
void CreateGUI( GUIContext** gui, Solver* solver, Gfx* gfx );
void DestroyGUI( GUIContext** gui );
void ShowGUI( GUIContext* gui, const gfx::Camera& camera );

}}}///