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

    bxPhysics_CPlaneHandle   collisionSpace_createPlane    ( bxPhysics_CollisionSpace* cs, const Vector4& plane );
    bxPhysics_CBoxHandle     collisionSpace_createBox      ( bxPhysics_CollisionSpace* cs, const Vector3& pos, const Quat& rot, const Vector3& ext );
    bxPhysics_CSphereHandle  collisionSpace_createSphere   ( bxPhysics_CollisionSpace* cs, const Vector4& sph );
    //bxPhysics_CCapsuleHandle collisionSpace_createCapsule  ( bxPhysics_CollisionSpace* cs, const Vector3& a, const Vector3& b, float radius );
    //bxPhysics_CTriHandle     collisiosSpace_addTriangles( bxPhysics_CollisionSpace* cs, const Vector3* positions, int nPositions, const u16* indices, int nIndices );
    
    void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_CPlaneHandle** ch );
    void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_CBoxHandle** ch );
    void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_CSphereHandle** ch );
    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_CCapsuleHandle** ch );
    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_CTriHandle** ch );

    //void collisionSpace_newFrame( bxPhysics_CollisionSpace* cs );
    void collisionSpace_collide( bxPhysics_CollisionSpace* cs, Vector3* points, int nPoints );
    void collisionSpace_detect( bxPhysics_CollisionSpace* cs );
    void collisionSpace_resolve( bxPhysics_CollisionSpace* cs );
    void collisionSpace_debugDraw( bxPhysics_CollisionSpace* cs );




    /// tmp solution
    extern bxPhysics_CollisionSpace* __cspace;
}///
