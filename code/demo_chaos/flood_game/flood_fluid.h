#pragma once

#include <util/array.h>
#include <util/vector.h>
#include <util/vectormath/vectormath.h>
#include "../spatial_hash_grid.h"

#include "flood_helpers.h"


namespace bx{ namespace flood{

typedef array_t<u32> Indices;
typedef array_t<size_t> MapCells;



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FluidNeighbourSearch
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

    static const u32 INITIAL_NEIGHBOUR_COUNT = 16;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct StaticBody
{
    const NeighbourIndices GetNeighbours  ( const Vector3F& posWS ) const;
    const Vector3F&        GetPosition    ( u32 index )             const { return _x[index]; }
          float            GetBoundaryPsi ( u32 index )             const { return _boundary_psi[index]; }

    // ---
    vec_float4 _map_cell_size_inv_vec;
    f32 _map_cell_size_inv = 0.f;
    f32 _particle_radius = 0.f;
    f32 _map_cell_size = 0.f;

    array_t<Vector3F> _x;
    array_t<f32> _boundary_psi;
    
    /*
    * this map maps particle world position to list of neighbors
    */
    hashmap_t _map;
    vector_t<Indices> _cell_neighbour_list;
};
void StaticBodyCreateBox( StaticBody* body, u32 countX, u32 countY, u32 countZ, float particleRadius, const Matrix4F& toWS );
void StaticBodyDoNeighbourMap( StaticBody* body, float supportRadius );
void StaticBodyDebugDraw( const StaticBody& body, u32 color );
void StaticBodyComputeBoundaryPsi( StaticBody* sbody, float density0 );

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

    f32 _dt_acc = 0.f;

    FluidNeighbourSearch _neighbours;

    struct Debug
    {
        bool show_density = true;
    }_debug;

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
    f32 time_step = 0.005f;
    i32 solver_iterations = 4;
    i32 max_steps_per_frame = 4;
};

void FluidCreateBox( Fluid* f, u32 width, u32 height, u32 depth, float particleRadius, const Matrix4F& pose );
void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime );


}}//