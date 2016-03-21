#pragma once

#include <util/vectormath/vectormath.h>

struct bxGdiContextBackend;

namespace bx
{
    struct Engine;
    struct PhxContacts;
    struct GameScene;
    struct Terrain;

    void terrainCreate( Terrain** terr, GameScene* gameScene, bx::Engine* engine );
    void terrainDestroy( Terrain** terr, GameScene* gameScene, bx::Engine* engine );

    void terrainTick( Terrain* terr, GameScene* gameScene, bxGdiContextBackend* gdi, float deltaTime );
    void terrainCollide( PhxContacts* con, Terrain* terr, const Vector3* points, int nPoints, float pointRadius, const Vector4& bsphere );

}///