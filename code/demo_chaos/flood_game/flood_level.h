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
    const char* _name      = nullptr;
    gfx::Scene  _gfx_scene = nullptr;

    void StartUp( game_gfx::Deffered* gfx, const char* levelName );
    void ShutDown( game_gfx::Deffered* gfx );

    void Tick( const GameTime& time );
    void Render( rdi::CommandQueue* cmdq, const GameTime& time );

    f32 _world_scale   = 0.01f;
    u32 _volume_width  = 128;
    u32 _volume_height = 32;
    u32 _volume_depth  = 128;

    Vector4F _plane_right;
    Vector4F _plane_bottom;
    Vector4F _plane_left;
    //Vector4F _plane_top;
    Vector4F _plane_front;
    Vector4F _plane_back;

    Fluid _fluid;
    FluidSimulationParams _fluid_sim_params = {};
    StaticBody _boundary[6];

};

}}
