#pragma once

#include "../game.h"
#include "../renderer.h"
#include <util/camera.h>


namespace bx
{

//////////////////////////////////////////////////////////////////////////
struct TestGameData
{
    gfx::Renderer renderer;

    gfx::GeometryPass geometry_pass;
    gfx::ShadowPass shadow_pass;
    gfx::SsaoPass ssao_pass;
    gfx::LightPass light_pass;
    gfx::PostProcessPass post_pass;
};

//////////////////////////////////////////////////////////////////////////
class TestMainState : public GameState
{
public:
    TestMainState( TestGameData* data )
        : _data( data ) {}

    const char* GetName() const override { return "Main"; }

    void OnStartUp () override;
    void OnShutDown() override;
    void OnUpdate  ( const GameTime& time ) override;
    void OnRender  ( const GameTime& time, rdi::CommandQueue* cmdq ) override;

    TestGameData* _data = nullptr;
    gfx::Scene    _gfx_scene = nullptr;
    gfx::Camera *active_camera_camera = {};
    gfx::CameraInputContext _camera_input_ctx = {};
};

//////////////////////////////////////////////////////////////////////////
class TestGame : public Game
{
public:
    TestGame();
    virtual ~TestGame();

protected:
    void StartUpImpl() override;
    void ShutDownImpl() override;
private:
    TestGameData _data;
};

}