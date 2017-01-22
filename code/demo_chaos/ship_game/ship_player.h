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
    f32 _thrust = 0.f;

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
    Vector3 _up_dir{ 0.f, 1.f, 0.f };
    Matrix3 _rot = Matrix3::identity();

    f32 _yaw = 0.f;
    f32 _pitch = 0.f;
    f32 _roll = 0.f;
    f32 _thrust = 0.f;


    f32 _vel_damping = 0.3f;
    f32 _mass_inv = 1.f;
    f32 _min_altitude = 0.f;
    f32 _max_altitude = 10.f;

    PlayerInput _input{};

    void Tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime );
    void FixedTick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime );

    //Vector3 GetDirection() const { return fastRotate( _rot, Vector3::zAxis() ); }
    Vector3 GetDirection() const { return _rot.getCol2(); }
};

}}//
