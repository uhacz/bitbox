#pragma once

#include "..\game_simple.h"
#include "terrain.h"

namespace bx { namespace terrain {

struct TerrainGame : GameSimple
{
    virtual ~TerrainGame() {}

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

    terrain::Instance* _tinstance = nullptr;
};


}}//
