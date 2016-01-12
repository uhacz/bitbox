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
    PhxContext* phxContextGet( PhxScene* scene );

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
        u32 type = eCOUNT;
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

        PhxGeometry() = default;
    };

    struct PhxHeightField
    {
        const f32* samples = nullptr;
        f32 sampleValueConversion = 1.f;
      
        i32 numRows = 0;
        i32 numCols = 0;
        
        f32 rowScale = 1.f;
        f32 colScale = 1.f;
        f32 heightScale = 1.f;
        f32 thickness = 1.f;
    };

    struct PhxMaterial
    {
        f32 sfriction = 0.5f;
        f32 dfriction = 0.5f;
        f32 restitution = 0.1f;

        PhxMaterial( f32 sf, f32 df, f32 r )
            : sfriction( sf )
            , dfriction( df )
            , restitution( r )
        {}
    };

    struct PhxQueryHit
    {
        enum EMask : u32
        {
            ePOSITION = BIT_OFFSET( 0 ),
            eNORMAL = BIT_OFFSET( 1 ),
            eDISTANCE = BIT_OFFSET( 2 ),
            eALL = ePOSITION | eNORMAL | eDISTANCE,
        };
        Vector3 position = Vector3(0.f);
        Vector3 normal = Vector3(0.f);
        f32 distance = 0.f;
        PhxActor* actor = nullptr;
        PhxShape* shape = nullptr;
        u32 mask = eALL;
    };

    bool phxActorCreateDynamic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, float density, const PhxMaterial* material = nullptr, const Matrix4& shapeOffset = Matrix4::identity() );
    bool phxActorCreateStatic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, const PhxMaterial* material = nullptr, const Matrix4& shapeOffset = Matrix4::identity() );
    bool phxActorCreateHeightfield( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxHeightField& geometry, const PhxMaterial* material = nullptr, const Matrix4& shapeOffset = Matrix4::identity() );
    void phxActorDestroy( PhxActor** actor );
        
	Matrix4 phxActorPoseGet( PhxActor* actor );
	void phxActorPoseSet( PhxActor* actor, const Matrix4& pose, PhxScene* scene );
    void phxActorTargetPoseSet( PhxActor* actor, const Matrix4& pose, PhxScene* scene );
    void phxActorUpdateHeightField( PhxActor* actor, const PhxHeightField& geometry );
    
    void phxSceneActorAdd( PhxScene* scene, PhxActor** actors, int nActors );
}////

namespace bx
{
    struct PhxContacts;
    void phxContactsCreate( PhxContacts** c, int capacity );
    void phxContactsDestroy( PhxContacts** c );

    int  phxContactsPushBack( PhxContacts* con, const Vector3& normal, float depth, u16 index );
    int  phxContactsSize( PhxContacts* con );
    void phxContactsGet( PhxContacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i );
    void phxContactsClear( PhxContacts* con );

    int phxContactsCollide( PhxContacts* con, const PhxScene* scene, const Vector3* points, int nPoints, float pointRadius, const Vector4& bsphere );
    bool phxSweep( PhxQueryHit* hit, const PhxScene* scene, const PhxGeometry& geometry, const TransformTQ& worldPose, const Vector3& dir, float maxDistance );
}///
