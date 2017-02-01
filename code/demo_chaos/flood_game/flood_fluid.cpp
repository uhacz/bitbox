#include "flood_fluid.h"
#include <rdi/rdi_debug_draw.h>
#include "util/grid.h"
#include "util/common.h"

#include "SPHKernels.h"

namespace bx {namespace flood {

//////////////////////////////////////////////////////////////////////////
void FluidClear( Fluid* f )
{
    array::clear( f->x );
    array::clear( f->p );
    array::clear( f->v );
}

void FluidCreate( Fluid* f, u32 numParticles, float particleRadius )
{
    FluidClear( f );
    array::reserve( f->x      , numParticles );
    array::reserve( f->p      , numParticles );
    array::reserve( f->v      , numParticles );
    array::reserve( f->density, numParticles );
    array::reserve( f->lambda , numParticles );
    array::reserve( f->dpos   , numParticles );

    f->particle_radius = particleRadius;

    for( u32 i = 0; i < numParticles; ++i )
    {
        array::push_back( f->x, Vector3( 0.f ) );
        array::push_back( f->p, Vector3( 0.f ) );
        array::push_back( f->v, Vector3( 0.f ) );
        array::push_back( f->density, 0.f );
        array::push_back( f->lambda, 0.f );
        array::push_back( f->dpos, Vector3( 0.f ) );
    }
}

void FluidInitMass( Fluid* f )
{
    // each particle represents a cube with a side length of r		
    // mass is slightly reduced to prevent pressure at the beginning of the simulation
    const float diam = 2.0f * f->particle_radius;
    f->particle_mass = 0.8f * diam*diam*diam * f->density0;
}
void FluidInitBox( Fluid* f, const Matrix4& pose )
{
    float fa = ::pow( (float)f->NumParticles(), 1.f / 3.f );
    u32 a = (u32)fa;

    const float offset = -fa * 0.5f;
    const float spacing = f->particle_radius * 1.1f;

    const bxGrid grid( a, a, a );


    for( u32 z = 0; z < a; ++z )
    {
        for( u32 y = 0; y < a; ++y )
        {
            for( u32 x = 0; x < a; ++x )
            {
                const float fx = ( offset + x ) * spacing;
                const float fy = ( offset + y ) * spacing;
                const float fz = ( offset + z ) * spacing;

                Vector3 pos( fx, fy, fz );
                pos = mulAsVec4( pose, pos );

                const u32 index = grid.index( x, y, z );
                SYS_ASSERT( index < f->NumParticles() );

                f->x[index] = pos;
                f->p[index] = pos;
                f->v[index] = Vector3( 0.f );
            }
        }
    }
    FluidInitMass( f );
}

namespace pbd
{
    struct ParticleDensity
    {
        f32 value;
        f32 error;
    };

    ParticleDensity ComputeDensity( u32 particleIndex, const Vector3* x, u32 numParticles, float density0, float particleMass )
    {
        float density = particleMass * PBD::CubicKernel::W_zero();
        for( u32 i = 0; i < numParticles; ++i )
        {
            density += particleMass * PBD::CubicKernel::W( x[particleIndex] - x[i] );
        }
        
        ParticleDensity result;
        result.value = density;
        result.error = maxOfPair( density, density0 ) - density0;
        return result;
    }

    float computeLagrangeMultiplier( u32 particleIndex, float particleDensity, const Vector3* x, u32 numParticles, float density0, float particleMass )
    {
        const float eps = 1.0e-6f;

        // Evaluate constraint function
        const float C = maxOfPair( particleDensity / density0 - 1.0f, 0.0f );			// clamp to prevent particle clumping at surface

        float lambda = 0.f;

        if( C != 0.0f )
        {
            // Compute gradients dC/dx_j 
            float sum_grad_C2 = 0.0f;
            Vector3 gradC_i( 0.0f, 0.0f, 0.0f );

            for( u32 i = 0; i < numParticles; ++i )
            {
                const Vector3 gradC_j = -particleMass / density0 * PBD::CubicKernel::gradW( x[particleIndex] - x[i] );
                sum_grad_C2 += lengthSqr( gradC_j ).getAsFloat();
                gradC_i -= gradC_j;
            }
            sum_grad_C2 += lengthSqr( gradC_i ).getAsFloat();

            // Compute lambda
            lambda = -C / ( sum_grad_C2 + eps );
        }
        else
            lambda = 0.0f;
    
        return lambda;
    }
}//

void FluidComputeDensities( Fluid* f )
{
}

void FluidComputeXSPHViscosity( Fluid* f )
{
}

void FluidUpdateTimeStepSizeCFL( Fluid* f, const float minTimeStepSize, const float maxTimeStepSize )
{
}
void FluidConstraintProjection( Fluid* f )
{

}

void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime )
{
    const u32 n = f->NumParticles();
    for( u32 i = 0; i < n; ++i )
    {
        Vector3 v = f->v[i] + params.gravity * deltaTime;
        Vector3 p = f->x[i] + v*deltaTime;

        f->v[i] = v;
        f->p[i] = p;
    }

    for( u32 i = 0; i < n; ++i )
    {
        Vector3 p = f->p[i];

        for( u32 j = 0; j < colliders.num_planes; ++j )
        {
            const Vector4& plane = colliders.planes[j];
            const floatInVec d = dot( plane, Vector4( p, oneVec ) );
            Vector3 dpos = -plane.getXYZ() * minf4( d, zeroVec );

            p += dpos;
        }

        f->p[i] = p;
    }

    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    for( u32 i = 0; i < n; ++i )
    {
        f->v[i] = ( f->p[i] - f->x[i] ) * delta_time_inv;
        f->x[i] = f->p[i];
    }

    for( Vector3 pos : f->x )
    {
        rdi::debug_draw::AddBox( Matrix4::translation( pos ), Vector3( f->particle_radius ), 0x0000FFFF, 1 );
        //rdi::debug_draw::AddSphere( Vector4( pos, f->_particle_radius ), 0x0000FFFF, 1 );
    }
}

}}///