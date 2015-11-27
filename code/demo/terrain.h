#pragma once

#include <util/vectormath/vectormath.h>

namespace bx
{
    struct GameScene;

    struct Terrain;
    void terrainCreate( Terrain** terr, GameScene* gameScene );
    void terrainDestroy( Terrain** terr, GameScene* gameScene );

    void terrainTick( Terrain* terr, GameScene* gameScene, float deltaTime );

}///