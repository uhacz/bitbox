#pragma once

#include <util/vectormath/vectormath.h>

struct bxEngine;
struct bxGdiContextBackend;

namespace bx
{
    struct PhxContacts;
    struct GameScene;
    struct Terrain;

    void terrainCreate( Terrain** terr, GameScene* gameScene, bxEngine* engine );
    void terrainDestroy( Terrain** terr, GameScene* gameScene, bxEngine* engine );

    void terrainTick( Terrain* terr, GameScene* gameScene, bxGdiContextBackend* gdi, float deltaTime );
    void terrainCollide( PhxContacts* con, const Vector3* points, int nPoints, float pointRadius, const Vector4& bsphere );

}///