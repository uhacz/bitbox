#pragma once

#include "..\renderer_type.h"
#include "..\game_time.h"

#include "ship_player.h"
#include "ship_terrain.h"

#include <rdi\rdi_backend.h>


namespace bx{
    
struct GameGfxDeffered;

namespace ship{


struct Level
{
    const char*     _name = nullptr;
    gfx::Scene      _gfx_scene = nullptr;
    PlayerCamera    _player_camera;
    Player          _player;
    Terrain         _terrain;

    // enemies
    // collectibles
    // terrain

    void StartUp( GameGfxDeffered* gfx, const char* levelName );
    void ShutDown( GameGfxDeffered* gfx );

    void Tick( const GameTime& time );
    void Render( rdi::CommandQueue* cmdq, const GameTime& time );
};


}}//
