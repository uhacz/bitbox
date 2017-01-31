#pragma once
#include "..\renderer_type.h"
#include "..\game_time.h"
#include <rdi\rdi_backend.h>

#include <util/array.h>

namespace bx {

namespace game_gfx
{
    struct Deffered;
}//

namespace flood {

struct Fluid
{
    array_t<Vector3> x;
    array_t<Vector3> p;
    array_t<Vector3> v;

    f32 _particle_radius = 0.f;

    u32 NumParticles() const { return array::sizeu( x ); }
};

struct FluidColliders
{
    const Vector4* planes = nullptr;
    u32 num_planes = 0;
};
struct FluidSimulationParams
{
    Vector3 gravity{ 0.f, -9.82f, 0.f };
    f32 velocity_damping = 0.1f;
};

void FluidCreate( Fluid* f, u32 numParticles, float particleRadius );
void FluidInitBox( Fluid* f, const Matrix4& pose );
void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime );



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
