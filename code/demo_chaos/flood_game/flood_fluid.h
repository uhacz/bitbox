#pragma once

#include <util/array.h>
#include <util/vector.h>
#include <util/vectormath/vectormath.h>
#include <intrin.h>


namespace bx{ namespace flood{

typedef array_t<u32> Indices;
typedef array_t<size_t> MapCells;

struct NeighbourIndices
{
    const u32* data = nullptr;
    u32 size = 0;

    bool Ok() const { return data && size; }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct NeighbourSearch
{
    void FindNeighbours( const Vector3F* points, u32 numPoints );
    void SetCellSize( float value );
    const Indices& GetNeighbours( u32 index ) const;

    f32 _cell_size_inv = 0.f;

    
    hashmap_t _map;
    MapCells  _map_cells;

    vector_t<Indices> _point_neighbour_list;
    array_t <size_t>  _point_spatial_hash;

    u32 _num_points = 0;

    static const u32 INITIAL_NEIGHBOUR_COUNT = 96;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct StaticBody
{
    
    const NeighbourIndices GetNeighbours( const Vector3F& posWS ) const;
    const Vector3F& GetPosition( u32 index ) const { return _x[index]; }

    // ---
    vec_float4 _map_cell_size_inv_vec;
    f32 _map_cell_size_inv = 0.f;
    f32 _particle_radius = 0.f;
    f32 _map_cell_size = 0.f;

    array_t<Vector3F> _x;
    
    /*
    * this map maps particle world position to list of neighbors
    */
    hashmap_t _map;
    vector_t<Indices> _cell_neighbour_list;
};
void StaticBodyCreateBox( StaticBody* body, u32 countX, u32 countY, u32 countZ, float particleRadius, const Matrix4F& toWS );
void StaticBodyDoNeighbourMap( StaticBody* body, float supportRadius );
void StaticBodyDebugDraw( const StaticBody& body, u32 color );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct Fluid
{
    array_t<Vector3F> x;
    array_t<Vector3F> p;
    array_t<Vector3F> v;
    array_t<f32> density;
    array_t<f32> lambda;
    array_t<Vector3F> dpos;

    f32 particle_radius = 0.025f;
    f32 support_radius = 4.f * 0.025f;
    f32 particle_mass = 0.f;
    f32 density0 = 1000.f; // 6378.f;
    f32 viscosity = 0.02f;

    u32 _maxIterations = 16;
    f32 _maxError = 0.01f;

    NeighbourSearch _neighbours;

    u32 NumParticles() const { return array::sizeu( x ); }
};

struct FluidColliders
{
    const Vector4F* planes = nullptr;
    u32 num_planes = 0;

    const StaticBody* static_bodies = nullptr;
    u32 num_static_bodies = 0;
};
struct FluidSimulationParams
{
    Vector3F gravity{ 0.f, -9.82f, 0.f };
    f32 velocity_damping = 0.1f;
};

void FluidCreate( Fluid* f, u32 numParticles, float particleRadius );
void FluidInitBox( Fluid* f, const Matrix4F& pose );
void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime );

}}//