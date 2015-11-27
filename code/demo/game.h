#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <gfx/gfx.h>
#include "physics.h"

struct bxInput;
struct bxGfxCamera;
struct bxGdiContextBackend;

namespace bx
{
    struct GameScene;

    struct Character;
    Character* character_new();
    void character_delete( Character** character );

    void characterInit( Character* character, bxGdiDeviceBackend* dev, GameScene* scene, const Matrix4& worldPose );
    void characterDeinit( Character* character, bxGdiDeviceBackend* dev );
    void characterTick( Character* character, bxGdiContextBackend* gdi, GameScene* scene, const bxInput& input, float deltaTime );

    Matrix4 characterPoseGet( const Character* character );
    Vector3 characterUpVectorGet( const Character* character );
};

namespace bx
{
    void characterCameraFollow( bx::GfxCamera* camera, const Character* character, float deltaTime, int cameraMoved = 0 );
}///

//namespace bxGame
//{
//    struct Flock;
//    Flock* flock_new();
//    void flock_delete( Flock** flock );
//
//    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius );
//    void flock_loadResources( Flock* flock, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
//    
//    struct Predator
//    {
//        enum { eMAX_COUNT = 4, };
//        Vector3 pos[ eMAX_COUNT ];
//        Vector3 vel[ eMAX_COUNT ];
//
//        i32 count;
//    };
//    
//    void flock_tick( Flock* flock, float deltaTime );
//}///



namespace bx
{
    struct GraphActor
    {
        virtual ~GraphActor() {}

        virtual void load() {}
        virtual void unload() {}

        virtual void parallelTick() {}
        virtual void serialTick() {}

        
    };


}////