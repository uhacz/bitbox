#pragma once

#include <util/array.h>
#include <util/vectormath/vectormath.h>

namespace bx{ namespace flood{



struct NeighbourSearch
{
    union Key
    {
        __m128i vec;
        struct  
        {
            i32 x, y, z, w;
        };
    };
    static Key MakeKey( const __m128 point, const __m128 cellSizeInv )
    {
        static const __m128 _1111 = _mm_set1_ps( 1.f );
        static const __m128 _0000 = _mm_set1_ps( 0.f );
        __m128 point_in_grid = vec_mul( point, cellSizeInv );
        point_in_grid = vec_sel( vec_sub( point_in_grid, _1111 ), point_in_grid, vec_cmpge( point, _0000 ) );
        Key k;
        k.vec = _mm_cvtps_epi32( point_in_grid );
        return k;
    }
    
    f32 _cell_size_inv = 0.f;

    hashmap_t _map;

    

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