#pragma once

#include <util/vectormath/vectormath.h>
#include <system/input.h>

struct bxPhx_Contacts;
struct bxGfxCamera;

namespace bxGame
{
    struct Constraint
    {
        i16 i0;
        i16 i1;
        f32 d;
    };

    struct ParticleData
    {
        Vector3* pos0;
        Vector3* pos1;
        Vector3* vel;
        f32* mass;
        f32* massInv;

        void* memoryHandle;
        i32 size;
        i32 capacity;
    };
    void particleDataAllocate( ParticleData* data, int newcap );
    void particleDataFree( ParticleData* data );
    
    struct Character1
    {
        enum
        {
            eMAIN_BODY_PARTICLE_COUNT = 3,
            eMAIN_BODY_CONSTRAINT_COUNT = 3,
            eWHEEL_BODY_PARTICLE_COUNT = 8,

            eTOTAL_PARTICLE_COUNT = eMAIN_BODY_PARTICLE_COUNT + eWHEEL_BODY_PARTICLE_COUNT,

        };

        struct Body
        {
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

        Vector3 footPos;
        Vector3 upVector;
        Vector3 frontVector;
        Vector3 sideVector;

        ParticleData particles;
        Body mainBody;
        Body wheelBody;
        Constraint mainBodyConstraints[eMAIN_BODY_CONSTRAINT_COUNT];
        Input input;

        //struct Particles
        //{
        //    Vector3 pos0[ePARTICLE_COUNT];
        //    Vector3 pos1[ePARTICLE_COUNT];
        //    Vector3 vel[ePARTICLE_COUNT];
        //    f32 mass[ePARTICLE_COUNT];
        //    f32 massInv[ePARTICLE_COUNT];
        //};

        bxPhx_Contacts* contacts;
        f32 _dtAcc;
        f32 _jumpAcc;
    };

    namespace CharacterInternal
    {
        void collectInputData( Character1::Input* charInput, const bxInput& input, float deltaTime );
        void debugDraw( Character1* ch );

        void initMainBody( Character1* ch, const Matrix4& worldPose );
        void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime );
        void simulateFinalize( Character1* ch, float staticFriction, float dynamicFriction, float deltaTime );

        void computeCharacterPose( Character1* ch );



    }

}///