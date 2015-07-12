#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>

struct bxPhysics_CollisionSpace;
struct bxPhysics_Contacts;

//struct bxPhysics_HPlaneShape  { u32 h; };
//struct bxPhysics_HBoxShape    { u32 h; };
//struct bxPhysics_HCapsuleShape{ u32 h; };
//struct bxPhysics_HSphereShape { u32 h; };
//struct bxPhysics_HTriShape    { u32 h; };
struct bxPhysics_HShape { u32 h; };

namespace bxPhysics
{
    enum EShape
    {
        eSHAPE_SPHERE = 0,
        eSHAPE_CAPSULE,
        eSHAPE_BOX,
        eSHAPE_PLANE,

        eSHAPE_COUNT,
    };

    bxPhysics_CollisionSpace* collisionSpace_new();
    void collisionSpace_delete( bxPhysics_CollisionSpace** cs );

    bxPhysics_HShape collisionSpace_createPlane( bxPhysics_CollisionSpace* cs, const Vector4& plane );
    bxPhysics_HShape collisionSpace_createBox( bxPhysics_CollisionSpace* cs, const Vector3& pos, const Quat& rot, const Vector3& ext );
    bxPhysics_HShape collisionSpace_createSphere( bxPhysics_CollisionSpace* cs, const Vector4& sph );
    
    void collisionSpace_releaseShape( bxPhysics_CollisionSpace* cs, bxPhysics_HShape* h );
    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_HBoxShape* ch );
    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_HSphereShape* ch );

    void collisionSpace_collide( bxPhysics_CollisionSpace* cs, bxPhysics_Contacts* contacts, Vector3* points, int nPoints );
    void collisionSpace_collide( bxPhysics_CollisionSpace* cs, bxPhysics_Contacts* contacts, Vector3* points, int nPoints, const u16* indices, int nIndices );
    void collisionSpace_detect( bxPhysics_CollisionSpace* cs );
    void collisionSpace_resolve( bxPhysics_CollisionSpace* cs );
    void collisionSpace_debugDraw( bxPhysics_CollisionSpace* cs );

    ////
    ////
    bxPhysics_Contacts* contacts_new( int capacity );
    void contacts_delete( bxPhysics_Contacts** con );
    int contacts_pushBack( bxPhysics_Contacts* con, const Vector3& normal, float depth, u16 index0 );
    //int contacts_pushBack( bxPhysics_Contacts* con, const Vector3& normal, float depth, u16 index0, u16 index1 ); TODO
    int contacts_size( bxPhysics_Contacts* con );
    void contacts_get( bxPhysics_Contacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i );
    void contacts_clear( bxPhysics_Contacts* con );


    /// tmp solution
    extern bxPhysics_CollisionSpace* __cspace;
}///


