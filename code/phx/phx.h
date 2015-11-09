#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx
{
    struct PhxContext;
    struct PhxScene;
    typedef void PhxActor;
    typedef void PhxShape;

    bool phxContextStartup( PhxContext** phx, int maxThreads );
    void phxContextShutdown( PhxContext** phx );

    bool phxSceneCreate( PhxScene** scene, PhxContext* ctx );
    void phxSceneDestroy( PhxScene** scene );
    PhxContext* phxSceneContextGet( PhxScene* scene );

    void phxSceneSimulate( PhxScene* scene, float deltaTime );
    void phxSceneSync( PhxScene* scene );

    struct PhxGeometry
    {
        enum
        {
            eBOX = 0,
            eSPHERE,
            eCAPSULE,

            eCOUNT,
        };
        u32 type;
        union
        {
            struct
            {
                f32 extx;
                f32 exty;
                f32 extz;
            }box;
            struct  
            {
                f32 radius;
            }sphere;
            struct
            {
                f32 radius;
                f32 halfHeight;
            }capsule;
        };
        PhxGeometry( f32 ex, f32 ey, f32 ez )
            : type( eBOX )
        {
            box.extx = ex;
            box.exty = ey;
            box.extz = ez;
        }

        PhxGeometry( f32 r )
            : type( eSPHERE)
        {
            sphere.radius = r;
        }
        
        PhxGeometry( f32 r, f32 hh )
            : type( eCAPSULE )
        {
            capsule.radius = r;
            capsule.halfHeight = hh;
        }

        PhxGeometry()
            : type( eCOUNT )
        {}
    };

    struct PhxMaterial
    {
        f32 sfriction;
        f32 dfriction;
        f32 restitution;

        PhxMaterial()
            : sfriction( 0.5f )
            , dfriction( 0.5f )
            , restitution( 0.1f )
        {}

        PhxMaterial( f32 sf, f32 df, f32 r )
            : sfriction( sf )
            , dfriction( df )
            , restitution( r )
        {}
    };

    bool phxActorCreateDynamic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, float density, const PhxMaterial* material = nullptr, const Matrix4& shapeOffset = Matrix4::identity() );
    bool phxActorCreateStatic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, const PhxMaterial* material = nullptr, const Matrix4& shapeOffset = Matrix4::identity() );
    void phxActorDestroy( PhxActor** actor );

    void phxSceneActorAdd( PhxScene* scene, PhxActor** actors, int nActors );

}////
