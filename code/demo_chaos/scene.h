#pragma once

#include <gfx/gfx.h>
#include <engine/engine.h>

namespace bx{
    struct PhxScene;
}////

namespace bx
{
    struct GameScene
    {
        Scene _scene;
        GfxScene* gfx_scene() { return _scene.gfx; }
        PhxScene* phx_scene() { return _scene.phx; }
    };

    namespace game_scene
    {
        void startup( GameScene* scene, bx::Engine* engine );
        void shutdown( GameScene* scene, bx::Engine* engine );
    }///
}

