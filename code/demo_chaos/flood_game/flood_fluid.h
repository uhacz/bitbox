#pragma once

#include <util/array.h>
#include <util/vectormath/vectormath.h>
#include <intrin.h>

namespace bx{ namespace flood{



struct NeighbourSearch
{
    void FindNeighbours( const Vector3* points, u32 numPoints );
    void SetCellSize( float value );

    f32 _cell_size_inv = 0.f;

    typedef array_t<u32> Indices;
    
    hashmap_t        _map;
    array_t<size_t>  _map_cells;
    array_t<Indices> _point_neighbour_list;
    array_t<size_t>  _point_spatial_hash;

    u32 _num_points = 0;
};

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

    u32 _maxIterations = 100;
    f32 _maxError = 0.01f;

    NeighbourSearch _neighbours;

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