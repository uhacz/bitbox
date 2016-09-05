#include "scene.h"

namespace bx
{
    void game_scene::startup( GameScene* scene, bx::Engine* engine )
    {
        Scene::startup( &scene->_scene, engine );
    }

    void game_scene::shutdown( GameScene* scene, bx::Engine* engine )
    {
        Scene::shutdown( &scene->_scene, engine );
    }
}///
