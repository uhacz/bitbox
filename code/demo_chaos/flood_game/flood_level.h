#pragma once
#include "..\renderer_type.h"
#include "..\game_time.h"
#include <rdi\rdi_backend.h>

namespace bx {

namespace game_gfx
{
    struct Deffered;
}//

namespace flood {

struct Level
{
    const char*     _name = nullptr;
    gfx::Scene      _gfx_scene = nullptr;

    void StartUp( game_gfx::Deffered* gfx, const char* levelName );
    void ShutDown( game_gfx::Deffered* gfx );

    void Tick( const GameTime& time );
    void Render( rdi::CommandQueue* cmdq, const GameTime& time );
};

}}
