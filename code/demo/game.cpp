#include "game.h"
#include <util/vectormath/vectormath.h>

namespace bxGame
{
    struct Constraint
    {
        u16 p0;
        u16 p1;
        f32 len;
    };

    struct CharacterConstraints
    {
        Constraint* c;
        i32 size;
        i32 capacity;
    };

    struct CharacterParticles
    {
        Vector3* localPos;
      
        Vector3* pos0;
        Vector3* pos1;
        Vector3* velocity;

        i32 size;
        i32 capacity;
    };

    struct CharacterCenterOfMass
    {
        Vector3 pos;
        Quat rot;
        
        Vector3 vel;
        Vector3 avel;
        
        Vector3 force;
        Vector3 torque;

        Vector3 inertia;
        f32 massInv;
    };

    struct Character
    {
        CharacterParticles particles;
        CharacterCenterOfMass centerOfMass;
    };

}///

namespace bxGame
{
    Character* character_new()
    {

    }
    void character_delete( Character** c )
    {
    
    }

    void character_init( Character* character, const Matrix4& worldPose )
    {

    }

    void character_tick( Character* character, float deltaTime )
    {

    }
}///

