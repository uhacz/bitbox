#pragma once

#include <util/vectormath/vectormath.h>
#include "../renderer_camera.h"

struct bxWindow;

namespace bx{ namespace ship{


struct Terrain;
struct Player;


struct CameraInput
{
    f32 _analog_x = 0.f;
    f32 _analog_y = 0.f;

    void Collect( bxWindow* win, float deltaTime, float inputRC = 0.1f );
};

struct PlayerInput
{
    f32 _analog_x = 0.f;
    f32 _analog_y = 0.f;
    f32 _fire = 0.f;
    f32 _altitude = 0.f;

    void Collect( bxWindow* win, float deltaTime, float inputRC = 0.1f );
};

//////////////////////////////////////////////////////////////////////////
struct PlayerCamera
{
    Vector3 _up_dir{ 0.f, 1.f, 0.f };
    f32 _distance_from_player = 1.f;
    
    gfx::Camera _camera{};
    CameraInput _input{};

    void Tick( const Player& player, const Terrain& terrain, float deltaTime );
};

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
struct Player
{
    Vector3 _pos{ 0 };
    Vector3 _vel{ 0 };
    Vector3 _acc{ 0 };
    Vector3 _up_dir{ 0.f, 1.f, 0.f };
    Quat _rot = Quat::identity();

    f32 _min_altitude = 1.f;
    f32 _max_altitude = 2.f;

    PlayerInput _input{};

    void Tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime );
    void FixedTick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime );

    Vector3 GetDirection() const { return fastRotate( _rot, Vector3::zAxis() ); }
};

}}//
