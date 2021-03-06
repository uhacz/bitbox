#pragma once

#include "../game_simple.h"

#include <util/camera.h>

namespace bx{ namespace flood{

struct FloodGame : GameSimple
{
    virtual ~FloodGame() {}

protected:
    void StartUpImpl() override;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct Level;
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

    game_gfx::Deffered* _gfx = nullptr;
    Level*              _level = nullptr;
};

}}//
