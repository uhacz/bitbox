#pragma once

#include "../game.h"
#include "../renderer.h"
#include <util/camera.h>

namespace bx
{
//////////////////////////////////////////////////////////////////////////
struct ShipGameGfx
{
    gfx::Renderer renderer;

    gfx::GeometryPass geometry_pass;
    gfx::ShadowPass shadow_pass;
    gfx::SsaoPass ssao_pass;
    gfx::LightPass light_pass;
    gfx::PostProcessPass post_pass;
};

//////////////////////////////////////////////////////////////////////////
class ShipLevelState : public GameState
{
public:
    ShipLevelState( ShipGameGfx* g )
        : _gfx( g ) {}

    const char* GetName() const override { return "Level"; }

    void OnStartUp() override;
    void OnShutDown() override;
    void OnUpdate( const GameTime& time ) override;
    void OnRender( const GameTime& time, rdi::CommandQueue* cmdq ) override;
        
    ShipGameGfx*  _gfx = nullptr;
    gfx::Scene    _gfx_scene = nullptr;
        
    gfx::Camera _main_camera = {};
    gfx::Camera _dev_camera = {};
    gfx::CameraInputContext _dev_camera_input_ctx = {};
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
    bool PreUpdateImpl( const GameTime& time ) override;

private:
    ShipGameGfx _gfx;
};
}//