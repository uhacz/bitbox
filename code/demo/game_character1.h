#pragma once

#include <util/vectormath/vectormath.h>
#include <system/input.h>

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

    struct Character1
    {
        enum
        {
            eMAIN_BODY_PARTICLE_COUNT = 3,
            eMAIN_BODY_CONSTRAINT_COUNT = 3,
            
            eWHEEL_BODY_PARTICLE_COUNT = 10,
            eWHEEL_BODY_CONSTRAINT_COUNT = 7,

            eTOTAL_PARTICLE_COUNT = eMAIN_BODY_PARTICLE_COUNT + eWHEEL_BODY_PARTICLE_COUNT,

        };

        struct CenterOfMass
        {
            Vector3 pos;
            Quat rot;
        };

        struct Body
        {
            CenterOfMass com;
            i16 begin;
            i16 end;

            i16 count() const { return end - begin; }
        };

        struct Input
        {
            f32 analogX;
            f32 analogY;
            f32 jump;
            f32 crouch;
        };

        Vector3 feetCenterPos;
        Vector3 upVector;
        Vector3 frontVector;
        Vector3 sideVector;
        
        Vector3 leftFootPos;
        Vector3 rightFootPos;

        ParticleData particles;

        Body mainBody;
        Body wheelBody;

        Vector3 wheelRestPos[eWHEEL_BODY_PARTICLE_COUNT];
        Constraint mainBodyConstraints[eMAIN_BODY_CONSTRAINT_COUNT];
        Constraint wheelBodyConstraints[eWHEEL_BODY_CONSTRAINT_COUNT];

        Input input;

        bxPhx_Contacts* contacts;
        f32 _dtAcc;
        f32 _jumpAcc;
    };

    namespace CharacterInternal
    {
        void collectInputData( Character1::Input* charInput, const bxInput& input, float deltaTime );
        void debugDraw( Character1* ch );

        void initMainBody( Character1* ch, const Matrix4& worldPose );
        void initWheelBody( Character1* ch, const Matrix4& worldPose );

        void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime );
        void simulateWheelBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime );
        void simulateWheelUpdatePose( Character1* ch, float shapeScale, float shapeStiffness );
        void simulateFinalize( Character1* ch, float staticFriction, float dynamicFriction, float deltaTime );

        void computeCharacterPose( Character1* ch );



    }

}///