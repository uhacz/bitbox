#pragma once

#include <util/vectormath/vectormath.h>

namespace bx
{
    struct Terrain;
    void terrainCreate( Terrain** terr );
    void terrainDestroy( Terrain** terr );

    void terrainTick( Terrain* terr, const Vector3& playerPosition, float deltaTime );

}///