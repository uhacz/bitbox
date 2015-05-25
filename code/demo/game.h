#pragma once

namespace bxGame
{
    struct Character;

    Character* character_new();
    void character_delete( Character** character );
    
    void character_init( Character* character, const Matrix4& worldPose );
    void character_tick( Character* character, float deltaTime );
    
}///
