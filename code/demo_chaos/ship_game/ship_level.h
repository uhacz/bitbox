#pragma once
#include "ship_player.h"
#include "ship_terrain.h"
#include "..\renderer_type.h"

namespace bx{namespace ship{

struct Gfx;

struct Level
{
    const char*     _name = nullptr;
    gfx::Scene      _gfx_scene = nullptr;
    PlayerCamera    _player_camera;
    Player          _player;

    // enemies
    // collectibles
    // terrain

    void StartUp( Gfx* gfx, const char* levelName );
    void ShutDown( Gfx* gfx );
};


}}//
