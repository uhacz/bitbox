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
    
}///


