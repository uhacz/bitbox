#pragma once

#include <util/vectormath/vectormath.h>

struct bxPhysics_Context;
struct bxPhysics_Scene;
struct bxPhysics_Actor;
struct bxPhysics_Shape;
//struct bxPhysics_Fluid;
//struct bxPhysics_Character;

struct bxPhysics_Transform
{
    Vector3 pos;
    Quat rot;

    bxPhysics_Transform() {}

    bxPhysics_Transform( const Vector3& p )
        : pos(p), rot( Quat::identity() ){
    }

    bxPhysics_Transform( const Quat& q )
        : pos( 0.f ), rot( q ){
    }

    bxPhysics_Transform( const Vector3& p, const Quat& q )
        : pos( p ), rot( q ){
    }

    bxPhysics_Transform( const Matrix4& pose )
        : pos( pose.getTranslation() ), rot( pose.getUpper3x3() ){
    }

    static bxPhysics_Transform identity() {
        return bxPhysics_Transform( Vector3( 0.f ), Quat::identity() ); 
    }
};

inline Matrix4 toMatrix4( const bxPhysics_Transform& tr ){
    return Matrix4( tr.rot, tr.pos );
}

namespace bxPhysics
{
    
    
    bxPhysics_Context* context_new();
    void context_delete( bxPhysics_Context** ctx );

    ////
    bxPhysics_Scene* scene_new( bxPhysics_Context* ctx );
    void scene_delete( bxPhysics_Scene** scene );
    void scene_simulate( bxPhysics_Scene* scene, float deltaTime );
    
    ////
    bxPhysics_Shape* shape_newBox     ( bxPhysics_Context* ctx, float halfWidth, float halfHeight, float halfDepth );
    bxPhysics_Shape* shape_newSphere  ( bxPhysics_Context* ctx, float radius );
    bxPhysics_Shape* shape_newCapsule ( bxPhysics_Context* ctx, float radius, float halfHeight );
    bxPhysics_Shape* shape_newPlane   ( bxPhysics_Context* ctx, const Vector3& normal );
    void shape_delete( bxPhysics_Context* ctx, bxPhysics_Shape** shape );
    
    ////
    bxPhysics_Actor* actor_new( bxPhysics_Context* ctx );
    void actor_delete( bxPhysics_Actor** actor );
    void actor_addShape( bxPhysics_Actor* actor, bxPhysics_Shape* shape, const bxPhysics_Transform& localPose = bxPhysics_Transform::identity() );

    bxPhysics_Transform actor_poseGet( bxPhysics_Actor* actor );
    void                actor_poseSet( bxPhysics_Actor* actor, const bxPhysics_Transform& pose );

    // forces acting at center of mass
    void actor_addForce( bxPhysics_Actor* actor, const Vector3& value );
    void actor_addTorque( bxPhysics_Actor* actor, const Vector3& value );

    ////
    //bxPhysics_Character* character_new( bxPhysics_Context* ctx );
    //void character_delete( bxPhysics_Character** character );
}///
