#pragma once
#include "..\renderer_type.h"
#include "..\game_time.h"
#include "flood_fluid.h"
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

    f32 _world_scale   = 0.01f;
    u32 _volume_width  = 1024;
    u32 _volume_height = 256;
    u32 _volume_depth  = 512;

    Vector4 _plane_right;
    Vector4 _plane_bottom;
    Vector4 _plane_left;
    Vector4 _plane_top;
    Vector4 _plane_front;
    Vector4 _plane_back;

    Fluid _fluid;
};

}}
