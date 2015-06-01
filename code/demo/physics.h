#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>

struct bxPhysics_CollisionSpace;
struct bxPhysics_CPlaneHandle  { u32 h; };
struct bxPhysics_CBoxHandle    { u32 h; };
struct bxPhysics_CCapsuleHandle{ u32 h; };
struct bxPhysics_CSphereHandle { u32 h; };
struct bxPhysics_CTriHandle    { u32 h; };
namespace bxPhysics
{
    bxPhysics_CollisionSpace* collisionSpace_new();
    void collisionSpace_delete( bxPhysics_CollisionSpace** cs );

    bxPhysics_CPlaneHandle   collisionSpace_addPlane     ( bxPhysics_CollisionSpace* cs, const Vector4& plane );
    bxPhysics_CBoxHandle     collisionSpace_addBox         ( bxPhysics_CollisionSpace* cs, const Vector3& pos, const Quat& rot, const Vector3& ext );
    bxPhysics_CCapsuleHandle collisionSpace_addCapsule ( bxPhysics_CollisionSpace* cs, const Vector3& a, const Vector3& b, float radius );
    bxPhysics_CSphereHandle  collisionSpace_addSphere   ( bxPhysics_CollisionSpace* cs, const Vector4& sph );
    bxPhysics_CTriHandle     collisiosSpace_addTriangles   ( bxPhysics_CollisionSpace* cs, const Vector3* positions, int nPositions, const u16* indices, int nIndices );

    void collisionSpace_remove( bxPhysics_CollisionSpace* cs, bxPhysics_CPlaneHandle** ch );
    void collisionSpace_remove( bxPhysics_CollisionSpace* cs, bxPhysics_CBoxHandle** ch );
    void collisionSpace_remove( bxPhysics_CollisionSpace* cs, bxPhysics_CCapsuleHandle** ch );
    void collisionSpace_remove( bxPhysics_CollisionSpace* cs, bxPhysics_CSphereHandle** ch );
    void collisionSpace_remove( bxPhysics_CollisionSpace* cs, bxPhysics_CTriHandle** ch );
}///
