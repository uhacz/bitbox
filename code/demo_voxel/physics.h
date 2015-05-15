#pragma once

#include <util/vectormath/vectormath.h>

struct bxPhysics_Context;
struct bxPhysics_Scene;
struct bxPhysics_Actor;
struct bxPhysics_Fluid;
struct bxPhysics_Character;

namespace bxPhysics
{
    bxPhysics_Context* context_new();
    void context_delete( bxPhysics_Context** ctx );

    ////
    bxPhysics_Scene* scene_new( bxPhysics_Context* ctx );
    void scene_delete( bxPhysics_Scene** scene );
    void scene_simulate( bxPhysics_Scene* scene, float deltaTime );

    ////
    bxPhysics_Actor* actor_new( bxPhysics_Context* ctx );
    void actor_delete( bxPhysics_Actor** actor );

    void actor_addBox    ( bxPhysics_Actor* actor, const Vector3& extents, float density, const Matrix4& localPose = Matrix4::identity() );
    void actor_addSphere ( bxPhysics_Actor* actor, float radius, float density, const Matrix4& localPose = Matrix4::identity() );
    void actor_addCapsule( bxPhysics_Actor* actor, float radius, float halfHeight, float density, const Matrix4& localPose = Matrix4::identity() );

    Matrix4 actor_poseGet( bxPhysics_Actor* actor );
    void    actor_poseSet( bxPhysics_Actor* actor, const Matrix4& pose );

    // forces acting at center of mass
    void actor_addForce( bxPhysics_Actor* actor, const Vector3& value );
    void actor_addTorque( bxPhysics_Actor* actor, const Vector3& value );

    ////
    bxPhysics_Character* character_new( bxPhysics_Context* ctx );
    void character_delete( bxPhysics_Character** character );
}///
