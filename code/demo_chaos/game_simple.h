#pragma once
#include "game.h"
#include "game_gfx.h"

namespace bx
{

class GameSimple : public Game
{
public:
    GameSimple();
    ~GameSimple();

protected:
    void StartUpImpl() override;
    void ShutDownImpl() override;
    bool PreUpdateImpl( const GameTime& time ) override;
    void PreRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq ) override;
    void PostRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq ) override;

protected:
    game_gfx::Deffered _gfx;
};

}//