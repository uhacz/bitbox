#pragma once

#include <util/vectormath/vectormath.h>
#include <system/input.h>
#include "renderer.h"

struct bxPhx_Contacts;
struct bxGfxCamera;

namespace bxGame
{
    struct Constraint
    {
        u16 i0;
        u16 i1;
        f32 d;
    };


    struct ParticleData
    {
        void* memoryHandle;
        i32 size;
        i32 capacity;
        
        Vector3* pos0;
        Vector3* pos1;
        Vector3* vel;
        f32* mass;
        f32* massInv;

        static void alloc( ParticleData* data, int newcap );
        static void free( ParticleData* data );
    };

    struct ConstraintData
    {
        void* memoryHandle;
        i32 size;
        i32 capacity;

        Constraint* desc;

        static void alloc( ConstraintData* data, int newcap );
        static void free( ConstraintData* data );
    };

    struct BodyRestPosition
    {
        void* memoryHandle;
        i32 size;

        Vector3* box;
        Vector3* sphere;
        Vector3* current;

        static void alloc( BodyRestPosition* data, int newcap );
        static void free( BodyRestPosition* data );
    };

    struct MeshVertex
    {
        f32 pos[3];
        f32 nrm[3];
    };

    struct MeshData
    {
        u16* indices;
        i32 nIndices;

        static void alloc( MeshData* data, int newcap );
        static void free( MeshData* data );
    };



    struct Character
    {
        struct CenterOfMass
        {
            Vector3 pos;
            Quat rot;
        };

        struct Body
        {
            CenterOfMass com;
            i16 particleBegin;
            i16 particleEnd;

            i16 constraintBegin;
            i16 constraintEnd;

            i16 particleCount() const { return particleEnd - particleBegin; }
            i16 constaintCount() const{ return constraintEnd - constraintBegin; }
        };

        struct Input
        {
            f32 analogX;
            f32 analogY;
            f32 jump;
            f32 crouch;
            f32 L2;
            f32 R2;
        };

        //Vector3 feetCenterPos;
        Vector3 upVector;
        //Vector3 frontVector;
        //Vector3 sideVector;
        
        Vector3 leftFootPos;
        Vector3 rightFootPos;

        ParticleData particles;
        ConstraintData constraints;
        BodyRestPosition restPos;
        MeshData shapeMeshData;
        //Body mainBody;
        Body shapeBody;
        
        bxGfx_HMeshInstance shapeMeshI;

        //Vector3 wheelRestPos[eWHEEL_BODY_PARTICLE_COUNT];
        //Constraint mainBodyConstraints[eMAIN_BODY_CONSTRAINT_COUNT];
        //Constraint wheelBodyConstraints[eWHEEL_BODY_CONSTRAINT_COUNT];

        Input input;

        bxPhx_Contacts* contacts;
        f32 _dtAcc;
        f32 _jumpAcc;
        f32 _shapeBlendAlpha;
    };

    namespace CharacterInternal
    {
        void collectInputData( Character::Input* charInput, const bxInput& input, float deltaTime );
        void debugDraw( Character* ch );

        //void initMainBody( Character1* ch, const Matrix4& worldPose );
        void initShapeBody( Character* ch, int shapeIterations, const Matrix4& worldPose );
        void initShapeMesh( Character* ch, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGfx_HWorld gfxWorld );
        void deinitShapeMesh( Character* ch );
        //void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime );
        void simulateShapeBodyBegin( Character* ch, const Vector3& extForce, float deltaTime );
        void simulateShapeUpdatePose( Character* ch, float shapeScale, float shapeStiffness );
        void simulateFinalize( Character* ch, float staticFriction, float dynamicFriction, float deltaTime );

        void updateMesh( MeshVertex* vertices, const Vector3* points, int nPoints, const u16* indices, int nIndices );

        void computeCharacterPose( Character* ch );
    }

}///