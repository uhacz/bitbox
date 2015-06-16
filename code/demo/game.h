#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>

struct bxInput;
struct bxGfxCamera;

namespace bxGame
{
    struct Character;

    Character* character_new();
    void character_delete( Character** character );
    
    void character_init( Character* character, const Matrix4& worldPose );
    void character_tick( Character* character, const bxGfxCamera& camera, const bxInput& input, float deltaTime );

    Matrix4 character_pose( const Character* character );
    Vector3 character_upVector( const Character* character );
}///

namespace bxGame
{
    void characterCamera_follow( bxGfxCamera* camera, const Character* character, float deltaTime, int cameraMoved = 0 );
}///

namespace bxGame
{
    struct Flock;
    Flock* flock_new();
    void flock_delete( Flock** flock );

    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius );
    void flock_addPredator( Flock* flock, const Matrix4& pose );

    void flock_tick( Flock* flock, float deltaTime );
}///



