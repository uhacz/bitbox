#pragma once

#include "../game.h"
#include "../renderer.h"
#include <util/camera.h>

#include "ship_player.h"

namespace bx
{
namespace ship
{

//////////////////////////////////////////////////////////////////////////
struct Gfx
{
    gfx::Renderer renderer;

    gfx::GeometryPass geometry_pass;
    gfx::ShadowPass shadow_pass;
    gfx::SsaoPass ssao_pass;
    gfx::LightPass light_pass;
    gfx::PostProcessPass post_pass;
};

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

//////////////////////////////////////////////////////////////////////////
class LevelState : public GameState
{
public:
    LevelState( Gfx* g )
        : _gfx( g ) {}

    const char* GetName() const override { return "Level"; }

    void OnStartUp() override;
    void OnShutDown() override;
    void OnUpdate( const GameTime& time ) override;
    void OnRender( const GameTime& time, rdi::CommandQueue* cmdq ) override;
        
    gfx::Camera             _dev_camera           = {};
    gfx::CameraInputContext _dev_camera_input_ctx = {};

    Gfx*   _gfx     = nullptr;
    Level* _level   = nullptr;

    bool _use_dev_camera = true;
};

//////////////////////////////////////////////////////////////////////////
class ShipGame : public Game
{
public:
    ShipGame();
    virtual ~ShipGame();

protected:
    void StartUpImpl() override;
    void ShutDownImpl() override;

private:
    Gfx _gfx;
};

}}//