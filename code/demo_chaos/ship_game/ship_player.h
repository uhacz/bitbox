#pragma once

#include <util/vectormath/vectormath.h>
#include "../renderer_camera.h"

struct bxWindow;

namespace bx{ namespace ship{


struct Terrain;
struct Player;



//////////////////////////////////////////////////////////////////////////
struct PlayerCamera
{
    gfx::Camera _camera;
    
    void tick( const Player& player, const Terrain& terrain, float deltaTime );
};

//////////////////////////////////////////////////////////////////////////
struct PlayerInput
{
    float _analog_x = 0.f;
    float _analog_y = 0.f;
    float _fire     = 0.f;
    
    void collect( bxWindow* win, float inputRC = 0.1f );
};

//////////////////////////////////////////////////////////////////////////
struct Player
{
    Vector3 _pos{ 0 };
    Vector3 _vel{ 0 };
    Vector3 _acc{ 0 };
    PlayerInput _input{};

    void tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime );
};

}}//
