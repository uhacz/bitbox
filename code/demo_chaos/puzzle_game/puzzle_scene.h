#pragma once

namespace bx{ namespace puzzle{


namespace physics
{
    struct Solver;
    struct Gfx;
}//

struct SceneCtx
{
    physics::Solver* phx_solver = nullptr;
    physics::Gfx*    phx_gfx = nullptr;
};

}}//