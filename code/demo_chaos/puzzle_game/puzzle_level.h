#pragma once

#include "..\game_simple.h"
#include "puzzle_player.h"
#include "puzzle_physics.h"

namespace bx { namespace puzzle{

struct PuzzleGame : GameSimple
{
    virtual ~PuzzleGame() {}

protected:
    void StartUpImpl() override;
};

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

    game_gfx::Deffered* _gfx = nullptr;
    gfx::Scene          _gfx_scene = nullptr;

    Player _player = {};
    physics::Solver* _solver = nullptr;
    physics::BodyId _rope[5];
};

}}//
