#include "phx.h"
#include <util/type.h>
#include <util/debug.h>
#include <util/memory.h>

#include "phx_physx_stepper.h"
#include "phx_physx_util.h"

#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>

namespace bx
{
namespace
{
    struct PhysxAllocator : public PxAllocatorCallback
    {
        void* allocate( size_t size, const char* typeName, const char* file, int line )
        {
            (void)typeName;
            (void)file;
            (void)line;
            return BX_MALLOC( bxDefaultAllocator(), size, 16 );
        }

        void deallocate( void* ptr )
        {
            BX_FREE( bxDefaultAllocator(), ptr );
        }
    };

    struct PhysxErrorCallback : public PxErrorCallback
    {
        PhysxErrorCallback() {}
        ~PhysxErrorCallback() {}

        virtual void reportError( PxErrorCode::Enum code, const char* message, const char* file, int line )
        {
            switch ( code )
            {
            case PxErrorCode::eDEBUG_INFO:
                bxLogInfo( message );
                break;
            case PxErrorCode::eDEBUG_WARNING:
            case PxErrorCode::ePERF_WARNING:
                bxLogWarning( message );
                break;
            case PxErrorCode::eINVALID_PARAMETER:
            case PxErrorCode::eINVALID_OPERATION:
            case PxErrorCode::eOUT_OF_MEMORY:
            case PxErrorCode::eINTERNAL_ERROR:
            case PxErrorCode::eABORT:
                bxLogError( message );
                break;
            default:
                break;
            }
        }
    };

    struct PhysxSimulationCallback : public PxSimulationEventCallback
    {
        virtual void onConstraintBreak( PxConstraintInfo* constraints, PxU32 count )
        {

        }
        virtual void onWake( PxActor** actors, PxU32 count )
        {

        }
        virtual void onSleep( PxActor** actors, PxU32 count )
        {

        }
        virtual void onContact( const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs )
        {

        }
        virtual void onTrigger( PxTriggerPair* pairs, PxU32 count )
        {

        }
    };
}///

struct PhxContext
{
    PhysxAllocator				allocator;
    PhysxErrorCallback			errorCallback;
    PxFoundation*				foundation = nullptr;
    PxPhysics*					physics = nullptr;
    PxCooking*					cooking = nullptr;
    PxMaterial*					defaultMaterial = nullptr;
    PxDefaultCpuDispatcher*		cpuDispatcher = nullptr;

    i32 flag_enableDebugDraw = 0;
    i32 flag_enableDebugDrawDepth = 1;
    
};

struct PhxScene
{
    enum
    {
        eSCRATCH_BUFFER_SIZE = 16 * 1024 * 4,
    };

    PhxContext*   ctx = nullptr;
    PxScene* scene = nullptr;
    PxControllerManager* controllerManager = nullptr;
    Stepper* stepper = nullptr;
    void* scratchBuffer = nullptr;

    PhysxSimulationCallback callback;
};

bool phxContextStartup( PhxContext** phx, int maxThreads )
{
    PhxContext* p = BX_NEW( bxDefaultAllocator(), PhxContext );
    
    p->foundation = PxCreateFoundation( PX_PHYSICS_VERSION, p->allocator, p->errorCallback );
    if ( !p->foundation )
        goto physx_start_up_error;

    p->physics = PxCreatePhysics( PX_PHYSICS_VERSION, *p->foundation, PxTolerancesScale() );
    if ( !p->physics )
        goto physx_start_up_error;

    if ( !PxInitExtensions( *p->physics ) )
        goto physx_start_up_error;

    p->cooking = PxCreateCooking( PX_PHYSICS_VERSION, *p->foundation, PxCookingParams( PxTolerancesScale() ) );
    if ( !p->cooking )
        goto physx_start_up_error;

    p->defaultMaterial = p->physics->createMaterial( 0.5f, 0.5f, 0.1f );
    if ( !p->defaultMaterial )
        goto physx_start_up_error;

    const int num_threads = 4;

    p->cpuDispatcher = PxDefaultCpuDispatcherCreate( num_threads );
    if ( !p->cpuDispatcher )
        goto physx_start_up_error;


    phx[0] = p;

    return true;

physx_start_up_error:
    bxLogError( "PhysX start up failed!" );
    return false;
}
void phxContextShutdown( PhxContext** phx )
{
    PhxContext* p = phx[0];

    const int n_scenes = p->physics->getNbScenes();
    if ( n_scenes )
    {
        bxLogError( "Please release all scenes before shutdown. Number of live scenes: %d", n_scenes );
        SYS_ASSERT( false );
    }

    releasePhysxObject( p->cpuDispatcher );
    releasePhysxObject( p->defaultMaterial );
    releasePhysxObject( p->cooking );
    PxCloseExtensions();
    releasePhysxObject( p->physics );
    releasePhysxObject( p->foundation );

    BX_DELETE0( bxDefaultAllocator(), phx[0] );
}

namespace
{
    PxFilterFlags physxDefaultSimulationFilterShader(
        PxFilterObjectAttributes attributes0,
        PxFilterData filterData0,
        PxFilterObjectAttributes attributes1,
        PxFilterData filterData1,
        PxPairFlags& pairFlags,
        const void* constantBlock,
        PxU32 constantBlockSize )
    {
        // let triggers through
        if ( PxFilterObjectIsTrigger( attributes0 ) || PxFilterObjectIsTrigger( attributes1 ) )
        {
            pairFlags = PxPairFlag::eTRIGGER_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_PERSISTS;
            return PxFilterFlag::eDEFAULT;
        }

        // trigger the contact callback for pairs (A,B) where
        // the filtermask of A contains the ID of B and vice versa.
        if ( (filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1) )
        {
            // generate contacts for all that were not filtered above
            //const bool a0Kinematic = PxFilterObjectIsKinematic( attributes0 );
            //const bool a1Kinematic = PxFilterObjectIsKinematic( attributes1 );

            //pairFlags = ( a0Kinematic && a1Kinematic ) ? PxPairFlags() : PxPairFlag::eCONTACT_DEFAULT;
            pairFlags = PxPairFlag::eCONTACT_DEFAULT;
            pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_CONTACT_POINTS;
        }

        return PxFilterFlag::eDEFAULT;
    }
}///

bool phxSceneCreate( PhxScene** scene, PhxContext* ctx )
{
    PxSceneDesc sdesc( ctx->physics->getTolerancesScale() );

    sdesc.gravity = toPxVec3( Vector3( 0.f, -9.82f, 0.f ) );
    sdesc.cpuDispatcher = ctx->cpuDispatcher;
    sdesc.filterShader = physxDefaultSimulationFilterShader;
    sdesc.nbContactDataBlocks = 16;
    PxScene* pxScene = ctx->physics->createScene( sdesc );
    if ( !pxScene )
    {
        return false;
    }

    
    pxScene->setVisualizationParameter( PxVisualizationParameter::eACTOR_AXES, 2.0f );
    pxScene->setVisualizationParameter( PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f );
    pxScene->setVisualizationParameter( PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 1.0f );
    //scene->setVisualizationParameter( PxVisualizationParameter::eCOLLISION_AXES, 1.0f );
    //scene->setVisualizationParameter( PxVisualizationParameter::eBODY_MASS_AXES, 1.0f );

    PhxScene* s = BX_NEW( bxDefaultAllocator(), PhxScene );
    s->ctx = ctx;
    s->scene = pxScene;
    s->controllerManager = PxCreateControllerManager( *pxScene );
    //scene->setSimulationEventCallback( &px_scene->callback );

    const f32 stepSize = 1.f / 120.f;
    const u32 maxSteps = 16;
    s->stepper = BX_NEW( bxDefaultAllocator(), FixedStepper, stepSize, maxSteps );

    s->scratchBuffer = BX_MALLOC( bxDefaultAllocator(), PhxScene::eSCRATCH_BUFFER_SIZE, 16 );

    scene[0] = s;
    return true;
}
void phxSceneDestroy( PhxScene** scene )
{
    PhxScene* s = scene[0];

    const int nActors = s->scene->getNbActors(
        PxActorTypeFlag::eRIGID_STATIC |
        PxActorTypeFlag::eRIGID_DYNAMIC |
        PxActorTypeFlag::ePARTICLE_SYSTEM |
        PxActorTypeFlag::ePARTICLE_FLUID |
        PxActorTypeFlag::eCLOTH );

    if ( nActors )
    {
        bxLogError( "%d actors in scene!", nActors );
        SYS_ASSERT( false );
    }

    //s->scene->setSimulationEventCallback( NULL );
    BX_FREE0( bxDefaultAllocator(), s->scratchBuffer );
    BX_DELETE0( bxDefaultAllocator(), s->stepper );
    releasePhysxObject( s->controllerManager );
    releasePhysxObject( s->scene );

    BX_DELETE0( bxDefaultAllocator(), scene[0] );
}
PhxContext* phxContextGet( PhxScene* scene )
{
    return scene->ctx;
}

namespace
{
    void debugDraw( PhxScene* s, int depth )
    {
        PxScene* scene = s->scene;
        const PxRenderBuffer& render_buffer = scene->getRenderBuffer();

        const u32 n_lines = render_buffer.getNbLines();
        const PxDebugLine* lines = render_buffer.getLines();
        for ( u32 i = 0; i < n_lines; ++i )
        {
            const PxDebugLine& dline = lines[i];
            bxGfxDebugDraw::addLine( toVector3( dline.pos0 ), toVector3( dline.pos1 ), dline.color0, depth );
        }
    }

    void guiDraw( PhxScene* s )
    {
        if( ImGui::Begin( "Physics" ) )
        {
            ImGui::Checkbox( "enable debug draw", (bool*)&s->ctx->flag_enableDebugDraw ); ImGui::SameLine();
            ImGui::Checkbox( "debug draw depth", (bool*)&s->ctx->flag_enableDebugDrawDepth );
        }
        ImGui::End();
    }
}///

void phxSceneSimulate( PhxScene* scene, float deltaTime )
{
    PxScene* pxScene = scene->scene;
    scene->stepper->advance( pxScene, deltaTime, scene->scratchBuffer, PhxScene::eSCRATCH_BUFFER_SIZE );
    scene->stepper->renderDone();
}

void phxSceneSync( PhxScene* scene )
{
    PxScene* pxScene = scene->scene;

    scene->stepper->wait( pxScene );

    if ( scene->ctx->flag_enableDebugDraw )
    {
        pxScene->setVisualizationParameter( PxVisualizationParameter::eSCALE, 1.0f );
        debugDraw( scene, scene->ctx->flag_enableDebugDrawDepth );
    }
    else
    {
        pxScene->setVisualizationParameter( PxVisualizationParameter::eSCALE, 0.0f );
    }

    guiDraw( scene );

}

////
//

struct PhxGeometryConversion
{
    PxBoxGeometry box;
    PxSphereGeometry sphere;
    PxCapsuleGeometry capsule;
    PxGeometry* geometry = nullptr;

    PhxGeometryConversion( const PhxGeometry& g )
    {
        switch( g.type )
        {
        case PhxGeometry::eBOX:
            {
                box.halfExtents = PxVec3( g.box.extx, g.box.exty, g.box.extz );
                geometry = &box;
            }break;
        case PhxGeometry::eSPHERE:
            {
                sphere.radius = g.sphere.radius;
                geometry = &sphere;
            }break;
        case PhxGeometry::eCAPSULE:
            {
                capsule.radius = g.capsule.radius;
                capsule.halfHeight = g.capsule.halfHeight;
                geometry = &capsule;
            }break;
        default:
            {
                SYS_NOT_IMPLEMENTED;
            }break;
        }///
        SYS_ASSERT( geometry != nullptr );
    }
};

void physxActorShapesFlagSet( PxRigidActor* actor, PxShapeFlag::Enum flag, bool yesNo )
{
    PxShape* shape = nullptr;
    u32 nShapes = actor->getNbShapes();
    for ( u32 i = 0; i < nShapes; ++i )
    {
        actor->getShapes( &shape, 1, i );
        shape->setFlag( flag, yesNo );
    }
}

void physxActorShapesCollisionGroupSet( PxRigidActor* actor, u32 selfMask, u32 collideWithMask )
{
    PxShape* shape = nullptr;
    u32 nShapes = actor->getNbShapes();
    for ( u32 i = 0; i < nShapes; ++i )
    {
        actor->getShapes( &shape, 1, i );
        PxFilterData fd = shape->getSimulationFilterData();
        fd.word0 = selfMask;
        fd.word1 = collideWithMask;
        shape->setSimulationFilterData( fd );
    }
}

bool phxActorCreateDynamic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, float density, const PhxMaterial* material, const Matrix4& shapeOffset )
{
    PxPhysics* sdk = ctx->physics;

    const PxTransform pxPose = toPxTransform( pose );
    const PxTransform pxShapeOffset = toPxTransform( shapeOffset );
    const PhxGeometryConversion geom( geometry );
    const float d = (density == 0.f) ? 10.f : ::abs( density );
    PxMaterial* pxMaterial = ctx->defaultMaterial;

    PxRigidDynamic* pxActor = PxCreateDynamic( *sdk, pxPose, *geom.geometry, *pxMaterial, d, pxShapeOffset );

    if ( !pxActor )
        return false;

    if( density <= 0.f )
    {
        pxActor->setRigidBodyFlag( PxRigidBodyFlag::eKINEMATIC, true );
    }

    pxActor->setActorFlag( PxActorFlag::eVISUALIZATION, true );
    physxActorShapesFlagSet( pxActor, PxShapeFlag::eVISUALIZATION, true );
    physxActorShapesCollisionGroupSet( pxActor, 1, 0xFFFFFFFF );
    actor[0] = pxActor;
    return true;
}

bool phxActorCreateStatic( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxGeometry& geometry, const PhxMaterial* material, const Matrix4& shapeOffset )
{
    PxPhysics* sdk = ctx->physics;

    const PxTransform pxPose = toPxTransform( pose );
    const PxTransform pxShapeOffset = toPxTransform( shapeOffset );
    const PhxGeometryConversion geom( geometry );
    PxMaterial* pxMaterial = ctx->defaultMaterial;

    PxRigidStatic* pxActor = PxCreateStatic( *sdk, pxPose, *geom.geometry, *pxMaterial, pxShapeOffset );

    if ( !pxActor )
        return false;

    pxActor->setActorFlag( PxActorFlag::eVISUALIZATION, true );
    physxActorShapesFlagSet( pxActor, PxShapeFlag::eVISUALIZATION, true );
    physxActorShapesCollisionGroupSet( pxActor, 1, 0xFFFFFFFF );

    actor[0] = pxActor;
    return true;
}

bool phxActorCreateHeightfield( PhxActor** actor, PhxContext* ctx, const Matrix4& pose, const PhxHeightField& geometry, const PhxMaterial* material /*= nullptr */ )
{
    PxPhysics* sdk = ctx->physics;

    const PxTransform pxPose = toPxTransform( pose );

    const int numSamples = geometry.numCols * geometry.numRows;
    const int sampleStride = sizeof( PxHeightFieldSample );
    PxHeightFieldSample* samples = ( PxHeightFieldSample*)BX_MALLOC( bxDefaultAllocator(), numSamples * sampleStride, ALIGNOF( PxHeightFieldSample ) );
    for( int i = 0 ; i < numSamples; ++i )
    {
        PxHeightFieldSample& sample = samples[i];
        sample.height = (i16)( geometry.samples[i] * geometry.sampleValueConversion );
        sample.materialIndex0 = 1;
        sample.materialIndex1 = 1;
        if( i % 2 )
            sample.setTessFlag();
    }

    PxHeightFieldDesc hfDesc;
    hfDesc.format = PxHeightFieldFormat::eS16_TM;
    hfDesc.samples.data = samples;
    hfDesc.samples.stride = sampleStride;
    hfDesc.nbRows = geometry.numRows;
    hfDesc.nbColumns = geometry.numCols;
    hfDesc.thickness = geometry.thickness;

    PxHeightField* hf = sdk->createHeightField( hfDesc );
    BX_FREE0( bxDefaultAllocator(), samples );

    if( !hf )
    {
        return false;
    }

    PxHeightFieldGeometry hfGeom( hf, PxMeshGeometryFlags(), geometry.heightScale, geometry.rowScale, geometry.colScale );

    PxRigidDynamic* pxActor = sdk->createRigidDynamic( pxPose );
    pxActor->setRigidBodyFlag( PxRigidBodyFlag::eKINEMATIC, true );

    PxMaterial* pxMaterial = ctx->defaultMaterial;
    PxShape* shape = pxActor->createShape( hfGeom, *pxMaterial, PxTransform::createIdentity() );
    if( !shape )
    {
        bxLogError( "Failed to create shape for heightfield actor!!" );
    }

    pxActor->setActorFlag( PxActorFlag::eVISUALIZATION, true );
    physxActorShapesFlagSet( pxActor, PxShapeFlag::eVISUALIZATION, true );
    physxActorShapesCollisionGroupSet( pxActor, 1, 0xFFFFFFFF );
    


    actor[0] = pxActor;
    return true;
}

void phxActorDestroy( PhxActor** actor )
{
    if ( !actor[0] )
        return;

    PxRigidActor* rigid = (PxRigidActor*)actor[0];
    rigid->release();

    actor[0] = nullptr;

}

void phxActorPoseSet( PhxActor* actor, const Matrix4& pose, PhxScene* scene )
{
    (void)scene;

    PxRigidActor* rigid = (PxRigidActor*)actor;
    SYS_ASSERT( rigid->getScene() == scene->scene );

    const PxTransform pxPose = toPxTransform( pose );
    rigid->setGlobalPose( pxPose );
}

void phxActorTargetPoseSet( PhxActor* actor, const Matrix4& pose, PhxScene* scene )
{
    (void)scene;
    PxRigidActor* rigid = (PxRigidActor*)actor;
    SYS_ASSERT( rigid->getScene() == scene->scene );
    SYS_ASSERT( rigid->isRigidDynamic() != nullptr );

    const PxTransform pxPose = toPxTransform( pose );
    PxRigidDynamic* rigidDynamic = (PxRigidDynamic*)rigid;
    rigidDynamic->setKinematicTarget( pxPose );    
}

void phxSceneActorAdd( PhxScene* scene, PhxActor** actors, int nActors )
{
    for( int i = 0; i < nActors; ++i )
    {
        PxActor* a = (PxActor*)actors[i];
        scene->scene->addActor( *a );
    }
}

}///


namespace bx
{
    int phxContactsCollide( PhxContacts* con, const PhxScene* scene, const Vector3* points, int nPoints, float pointRadius, const Vector4& bsphere )
    {
        const PxScene* pxscene = scene->scene;

        const PxSphereGeometry ovGeom( bsphere.getW().getAsFloat() );
        const PxTransform ovPose = toPxTransform( Matrix4::translation( bsphere.getXYZ() ) );

        const int N = 16;
        PxOverlapBufferN<N> ovBuffer;
        const bool hasHit = pxscene->overlap( ovGeom, ovPose, ovBuffer );
        if ( !hasHit )
            return 0;

        const int nHits = (int)ovBuffer.getNbAnyHits();

        int nCollisions = 0;
        const PxSphereGeometry pointGeom( pointRadius );
        for( int ip = 0; ip < nPoints; ++ip )
        {
            const PxTransform pointPose( toPxVec3( points[ip] ) );
            for ( int ih = 0; ih < nHits; ++ih )
            {
                const PxOverlapHit& ovHit = ovBuffer.getAnyHit( ih );
                const PxGeometryHolder hitGeom = ovHit.shape->getGeometry();
                const PxTransform hitPose = PxShapeExt::getGlobalPose( *ovHit.shape, *ovHit.actor );

                PxVec3 normal;
                float depth;
                bool penetration = PxGeometryQuery::computePenetration( normal, depth, pointGeom, pointPose, hitGeom.any(), hitPose );
                if( penetration )
                {
                    const Vector3 normalV3 = toVector3( normal );
                    phxContactsPushBack( con, normalV3, depth, (u16)ip );
                    //points[ip] += normalV3 * depth;
                    ++nCollisions;
                }
            }
        }

        return nCollisions;
    }
}///