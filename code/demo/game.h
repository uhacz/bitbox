#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <gfx/gfx.h>
#include "physics.h"

struct bxInput;
struct bxGfxCamera;
struct bxGdiContextBackend;
struct bxDemoScene;

namespace bxGame
{
    struct Character;
    Character* character_new();
    void character_delete( Character** character );

    void character_init( Character* character, bxGdiDeviceBackend* dev, bxDemoScene* scene, const Matrix4& worldPose );
    void character_deinit( Character* character, bxGdiDeviceBackend* dev );
    void character_tick( Character* character, bxGdiContextBackend* gdi, bxDemoScene* scene, const bxInput& input, float deltaTime );

    Matrix4 character_pose( const Character* character );
    Vector3 character_upVector( const Character* character );
};

namespace bxGame
{
    void characterCamera_follow( bx::GfxCamera* camera, const Character* character, float deltaTime, int cameraMoved = 0 );
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



