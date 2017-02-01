#pragma once

#include <util/array.h>
#include <util/vectormath/vectormath.h>

namespace bx{ namespace flood{

struct Fluid
{
    array_t<Vector3> x;
    array_t<Vector3> p;
    array_t<Vector3> v;
    array_t<f32> density;
    array_t<f32> lambda;
    array_t<Vector3> dpos;

    f32 particle_radius = 0.025f;
    f32 support_radius = 4.f * 0.025f;
    f32 particle_mass = 0.f;
    f32 density0 = 1000.f;
    f32 viscosity = 0.02f;

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

}}//