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
    PBD::CubicKernel::setRadius( f->support_radius );
}

//namespace pbd
//{
//    struct ParticleDensity
//    {
//        f32 value;
//        f32 error;
//    };
//
//    ParticleDensity ComputeDensity( u32 particleIndex, const Vector3* x, u32 numParticles, float density0, float particleMass )
//    {
//        float density = particleMass * PBD::CubicKernel::W_zero();
//        for( u32 i = 0; i < numParticles; ++i )
//        {
//            density += particleMass * PBD::CubicKernel::W( x[particleIndex] - x[i] );
//        }
//        
//        ParticleDensity result;
//        result.value = density;
//        result.error = maxOfPair( density, density0 ) - density0;
//        return result;
//    }
//
//    float ComputeLagrangeMultiplier( u32 particleIndex, float particleDensity, const Vector3* x, u32 numParticles, float density0, float particleMass )
//    {
//        const float eps = 1.0e-6f;
//
//        // Evaluate constraint function
//        const float C = maxOfPair( particleDensity / density0 - 1.0f, 0.0f );			// clamp to prevent particle clumping at surface
//
//        float lambda = 0.f;
//
//        if( C != 0.0f )
//        {
//            // Compute gradients dC/dx_j 
//            float sum_grad_C2 = 0.0f;
//            Vector3 gradC_i( 0.0f, 0.0f, 0.0f );
//
//            for( u32 i = 0; i < numParticles; ++i )
//            {
//                const Vector3 gradC_j = -particleMass / density0 * PBD::CubicKernel::gradW( x[particleIndex] - x[i] );
//                sum_grad_C2 += lengthSqr( gradC_j ).getAsFloat();
//                gradC_i -= gradC_j;
//            }
//            sum_grad_C2 += lengthSqr( gradC_i ).getAsFloat();
//
//            // Compute lambda
//            lambda = -C / ( sum_grad_C2 + eps );
//        }
//        else
//            lambda = 0.0f;
//    
//        return lambda;
//    }
//}//
//
//void FluidComputeDensities( Fluid* f )
//{
//}
//
//void FluidComputeXSPHViscosity( Fluid* f )
//{
//}
//
//void FluidUpdateTimeStepSizeCFL( Fluid* f, const float minTimeStepSize, const float maxTimeStepSize )
//{
//}
//void FluidConstraintProjection( Fluid* f )
//{
//
//}
static Fluid* g_fluid = nullptr;
void FluidSolvePressure( Fluid* f )
{
    g_fluid = f;
    const u32 num_particles = f->NumParticles();
    const float num_particles_inv = 1.f / num_particles;
    
    const float eps = 1.0e-6f;
    const float particle_mass = f->particle_mass;
    const float density0 = f->density0;
    const Vector3* x = array::begin( f->p );

    const float eta = f->_maxError * 0.01f * density0;
    u32 iterations = 0;
    float avg_density_err = 0.f;
    
    const float aa = f->support_radius * f->support_radius;

    while( ( ( avg_density_err > eta ) || ( iterations < 2 ) ) && ( iterations < f->_maxIterations ) )
    {
        avg_density_err = 0.f;

        for( u32 i = 0; i < num_particles; ++i )
        {
            // -- compute particle density
            float density = particle_mass * PBD::CubicKernel::W_zero();
            for( u32 j = 0; j < num_particles; ++j )
            {
                if( i == j )
                    continue;

                density += particle_mass * PBD::CubicKernel::W( x[i] - x[j] );
            }
            f->density[i] = density;
            avg_density_err += ( maxOfPair( density, density0 ) - density0 ) * num_particles_inv;
        
            // -- compute lambda
            // Evaluate constraint function
            const float C = maxOfPair( density / density0 - 1.0f, 0.0f );			// clamp to prevent particle clumping at surface

            float lambda = 0.f;

            if( C != 0.0f )
            {
                // Compute gradients dC/dx_j 
                float sum_grad_C2 = 0.0f;
                Vector3 gradC_i( 0.0f, 0.0f, 0.0f );

                for( u32 j = 0; j < num_particles; ++j )
                {
                    if( i == j )
                        continue;

                    const Vector3 gradC_j = -particle_mass / density0 * PBD::CubicKernel::gradW( x[i] - x[j] );
                    sum_grad_C2 += lengthSqr( gradC_j ).getAsFloat();
                    gradC_i -= gradC_j;
                }
                sum_grad_C2 += lengthSqr( gradC_i ).getAsFloat();

                // Compute lambda
                lambda = -C / ( sum_grad_C2 + eps );
            }
            else
            {
                lambda = 0.0f;
            }

            f->lambda[i] = lambda;
        }

        // -- position correction
        for( u32 i = 0; i < num_particles; ++i )
        {
            Vector3 corr( 0.f );

            const Vector3& xi = f->p[i];
            for( u32 j = 0; j < num_particles; ++j )
            {
                if( i == j )
                    continue;

                const Vector3& xj = f->p[j];

                if( lengthSqr( xj - xi ).getAsFloat() > aa )
                    continue;

                const Vector3 gradC_j = -particle_mass / density0 * PBD::CubicKernel::gradW( xi - xj );
                corr -= ( f->lambda[i] + f->lambda[j] ) * gradC_j;
            }
            f->dpos[i] = corr;
        }

        for( u32 i = 0; i < num_particles; ++i )
        {
            f->p[i] += f->dpos[i];
        }

        ++iterations;
    }
}

void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime )
{
    deltaTime = 0.005f;
    const u32 n = f->NumParticles();
    for( u32 i = 0; i < n; ++i )
    {
        Vector3 v = f->v[i] + params.gravity * deltaTime;
        Vector3 p = f->x[i] + v*deltaTime;

        f->v[i] = v;
        f->p[i] = p;
    }

    {
        FluidSolvePressure( f );
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