#include "game_simple.h"
#include "game_gui.h"

namespace bx
{

GameSimple::GameSimple()
{
}


GameSimple::~GameSimple()
{
}

void GameSimple::StartUpImpl()
{
    game_gui::StartUp();
    game_gfx::StartUp( &_gfx );
}

void GameSimple::ShutDownImpl()
{
    game_gfx::ShutDown( &_gfx );
    game_gui::ShutDown();
}

bool GameSimple::PreUpdateImpl( const GameTime& time )
{
    game_gui::NewFrame();
    return true;
}

void GameSimple::PreRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    _gfx.renderer.BeginFrame( cmdq );
}

void GameSimple::PostRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    game_gui::Render();
    _gfx.renderer.EndFrame( cmdq );
}

}