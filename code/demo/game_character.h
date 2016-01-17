#pragma once

#include <util/vectormath/vectormath.h>
#include <system/input.h>

struct bxGdiDeviceBackend;

namespace bx
{
    struct GfxScene;
    struct PhxContacts;
    struct GfxMeshInstance;
}///

namespace bx
{
    struct Constraint
    {
        u16 i0 = -1;
        u16 i1 = -1;
		f32 d = 0.f;
    };


    struct ParticleData
    {
        void* memoryHandle = nullptr;
        i32 size = 0;
        i32 capacity = 0;
        
        Vector3* pos0 = nullptr;
        Vector3* pos1 = nullptr;
        Vector3* vel = nullptr;
        f32* mass = nullptr;
        f32* massInv = nullptr;

        static void alloc( ParticleData* data, int newcap );
        static void free( ParticleData* data );
    };

    struct ConstraintData
    {
        void* memoryHandle = nullptr;
        i32 size = 0;
        i32 capacity = 0;

        Constraint* desc = nullptr;

        static void alloc( ConstraintData* data, int newcap );
        static void free( ConstraintData* data );
    };

    struct BodyRestPosition
    {
        void* memoryHandle = nullptr;
        i32 size = 0;

        Vector3* box = nullptr;
        Vector3* sphere = nullptr;
        Vector3* current = nullptr;

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
        u16* indices = nullptr;
		i32 nIndices = 0;

        static void alloc( MeshData* data, int newcap );
        static void free( MeshData* data );
    };
	
    struct Character
    {
        struct CenterOfMass
        {
            Vector3 pos = Vector3(0.f);
            Quat rot = Quat::identity();
        };

        struct Body
        {
            CenterOfMass com;
            i16 particleBegin = 0;
            i16 particleEnd = 0;

            i16 constraintBegin = 0;
            i16 constraintEnd = 0;

            i16 particleCount() const { return particleEnd - particleBegin; }
            i16 constaintCount() const{ return constraintEnd - constraintBegin; }
        };

        struct Input
        {
            f32 analogX = 0.f;
            f32 analogY = 0.f;
            f32 jump = 0.f;
            f32 crouch = 0.f;
            f32 L2 = 0.f;
            f32 R2 = 0.f;
        };

        Vector3 upVector = Vector3::yAxis();
                
        Vector3 leftFootPos = Vector3(0.f);
        Vector3 rightFootPos = Vector3(0.f);

        ParticleData particles;
        ConstraintData constraints;
        BodyRestPosition restPos;
        MeshData shapeMeshData;
        
        Body shapeBody;
        Input input;

        bx::GfxMeshInstance* meshInstance = nullptr;
        bx::PhxContacts* contacts = nullptr;
        f32 _dtAcc = 0.f;
        f32 _jumpAcc = 0.f;
        f32 _shapeBlendAlpha = 0.f;
    };

    namespace CharacterInternal
    {
        void collectInputData( Character::Input* charInput, const bxInput& input, float deltaTime );
        void debugDraw( Character* ch );

        void initShapeBody( Character* ch, int shapeIterations, const Matrix4& worldPose );
        void initShapeMesh( Character* ch, bxGdiDeviceBackend* dev, bx::GfxScene* gfxScene );
        void deinitShapeMesh( Character* ch, bxGdiDeviceBackend* dev );
        void simulateShapeBodyBegin( Character* ch, const Vector3& extForce, float deltaTime );
        void simulateShapeUpdatePose( Character* ch, float shapeScale, float shapeStiffness );
        void simulateFinalize( Character* ch, float staticFriction, float dynamicFriction, float deltaTime );
		void updateMesh( MeshVertex* vertices, const Vector3* points, int nPoints, const u16* indices, int nIndices );
		void computeCharacterPose( Character* ch );
    }

}///