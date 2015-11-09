#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>

struct bxPhx_CollisionSpace;
struct bxPhx_Contacts;

//struct bxPhysics_HPlaneShape  { u32 h; };
//struct bxPhysics_HBoxShape    { u32 h; };
//struct bxPhysics_HCapsuleShape{ u32 h; };
//struct bxPhysics_HSphereShape { u32 h; };
//struct bxPhysics_HTriShape    { u32 h; };
struct bxPhx_HShape { u32 h; };
struct bxPhx_ShapeBox
{
    Quat rot;
    Vector3 pos;
    Vector3 ext;
};

namespace bxPhx
{
    enum EShape
    {
        eSHAPE_SPHERE = 0,
        eSHAPE_CAPSULE,
        eSHAPE_BOX,
        eSHAPE_PLANE,

        eSHAPE_COUNT,
    };

    bxPhx_CollisionSpace* collisionSpace_new();
    void collisionSpace_delete( bxPhx_CollisionSpace** cs );

    bxPhx_HShape shape_createPlane( bxPhx_CollisionSpace* cs, const Vector4& plane );
    bxPhx_HShape shape_createBox( bxPhx_CollisionSpace* cs, const Vector3& pos, const Quat& rot, const Vector3& ext );
    bxPhx_HShape shape_createSphere( bxPhx_CollisionSpace* cs, const Vector4& sph );
    
    void shape_release( bxPhx_CollisionSpace* cs, bxPhx_HShape* h );
    Matrix4 shape_pose( bxPhx_CollisionSpace* cs, bxPhx_HShape h, Vector4* shapeParams = 0 );
    void shape_poseSet( bxPhx_CollisionSpace* cs, bxPhx_HShape h, const Matrix4& pose );
    /*TODO*/Vector4 shape_sphere( bxPhx_CollisionSpace* cs, bxPhx_HShape h );
    /*TODO*/Vector4 shape_plane( bxPhx_CollisionSpace* cs, bxPhx_HShape h );
    /*TODO*/bxPhx_ShapeBox shape_box( bxPhx_CollisionSpace* cs, bxPhx_HShape h );

    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_HBoxShape* ch );
    //void collisionSpace_release( bxPhysics_CollisionSpace* cs, bxPhysics_HSphereShape* ch );

    void collisionSpace_collide( bxPhx_CollisionSpace* cs, bxPhx_Contacts* contacts, Vector3* points, int nPoints );
    void collisionSpace_collide( bxPhx_CollisionSpace* cs, bxPhx_Contacts* contacts, Vector3* points, int nPoints, const u16* indices, int nIndices );
    void collisionSpace_detect( bxPhx_CollisionSpace* cs );
    void collisionSpace_resolve( bxPhx_CollisionSpace* cs );
    void collisionSpace_debugDraw( bxPhx_CollisionSpace* cs );

    
    ////
    ////
    bxPhx_Contacts* contacts_new( int capacity );
    void contacts_delete( bxPhx_Contacts** con );
    int contacts_pushBack( bxPhx_Contacts* con, const Vector3& normal, float depth, u16 index0 );
    //int contacts_pushBack( bxPhysics_Contacts* con, const Vector3& normal, float depth, u16 index0, u16 index1 ); TODO
    int contacts_size( bxPhx_Contacts* con );
    void contacts_get( bxPhx_Contacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i );
    void contacts_clear( bxPhx_Contacts* con );
}///


namespace bx
{
    struct PhxScene;
    struct PhxContacts;
    void phxContactsCreate( PhxContacts** c, int capacity );
    void phxContactsDestroy( PhxContacts** c );

    int  phxContactsPushBack( PhxContacts* con, const Vector3& normal, float depth, u16 index );
    int  phxContactsSize( PhxContacts* con );
    void phxContactsGet( PhxContacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i );
    void phxContactsClear( PhxContacts* con );

    void phxContactsCollide( PhxContacts* con, PhxScene* scene, const Vector3* points, int nPoints );
}///