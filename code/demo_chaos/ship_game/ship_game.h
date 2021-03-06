#pragma once

#include "../game.h"
#include "../game_gfx.h"
#include "../renderer.h"
#include <util/camera.h>

namespace bx
{
namespace ship
{


struct Level;
//////////////////////////////////////////////////////////////////////////
class LevelState : public GameState
{
public:
    LevelState( game_gfx::Deffered* g )
        : _gfx( g ) {}

    const char* GetName() const override { return "Level"; }

    void OnStartUp() override;
    void OnShutDown() override;
    void OnUpdate( const GameTime& time ) override;
    void OnRender( const GameTime& time, rdi::CommandQueue* cmdq ) override;
        
    game_gfx::Deffered* _gfx   = nullptr;
    Level*              _level = nullptr;
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
    void PreRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq ) override;
    void PostRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq ) override;

private:
    game_gfx::Deffered _gfx;
};

}}//