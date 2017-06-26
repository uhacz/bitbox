#include "puzzle_physics_util.h"
#include "puzzle_physics.h"
#include "puzzle_physics_pbd.h"

#include "sdf.h"
#include "voxelize.h"

#include <util/array.h>
#include <util/bbox.h>
#include <util/poly/poly_shape.h>
#include <util/random.h>

namespace bx{ namespace puzzle{
namespace physics{

BodyId CreateRope( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len, float particleMass )
{
    const float pradius = physics::GetParticleRadius( solver );

    const u32 num_points = (u32)( len / ( pradius * 2.f ) );
    const float step = ( len / (float)( num_points ) );
    const float particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    BodyId rope = physics::CreateBody( solver, num_points );

    Vector3F* points = MapPosition( solver, rope );
    f32* mass_inv = MapMassInv( solver, rope );
    
    points[0] = attach;
    mass_inv[0] = particle_mass_inv;
    for( u32 i = 1; i < num_points; ++i )
    {
        points[i] = points[i - 1] + axis * step;
        mass_inv[i] = particle_mass_inv;
    }

    physics::Unmap( solver, mass_inv );
    physics::Unmap( solver, points );

    array_t< DistanceCInfo >c_info;
    array::reserve( c_info, num_points - 1 );
    for( u32 i = 0; i < num_points - 1; ++i )
    {
        DistanceCInfo& info = c_info[i];
        info.i0 = i;
        info.i1 = i + 1;
    }
    SetDistanceConstraints( solver, rope, c_info.begin(), num_points - 1 );

    return rope;
}

BodyId CreateCloth( Solver* solver, const Vector3F& attach, const Vector3F& axis, float width, float height, float particleMass )
{
    return{0};
}


#if 0
static inline bool EdgeDetect( u32 ix, u32 iy, u32 iz, u32 w, u32 d, u32 h )
{
    bool is_on_edge = false;
    is_on_edge |= iz == 0 || iz == ( d - 1 );
    is_on_edge |= iy == 0 || iy == ( h - 1 );
    is_on_edge |= ix == 0 || ix == ( w - 1 );

    return is_on_edge;
}
BodyId CreateSoftBox( Solver* solver, const Matrix4F& pose, float width, float depth, float height, float particleMass, bool shell )
{
    const f32 pradius = physics::GetParticleRadius( solver );
    const f32 pradius2 = pradius * 2.f;

    const u32 w = (u32)( width / pradius2 );
    const u32 h = (u32)( height / pradius2 );
    const u32 d = (u32)( depth / pradius2 );
    
    u32 num_particles = 0;
    if( shell )
    {
        for( u32 iz = 0; iz < d; ++iz )
        {
            for( u32 iy = 0; iy < h; ++iy )
            {
                for( u32 ix = 0; ix < w; ++ix )
                {
                    if( !EdgeDetect( ix, iy, iz, w, h, d ) )
                        continue;

                    ++num_particles;
                }
            }
        }
    }
    else
    {
        num_particles = w*h*d;
    }

    //const u32 num_particles = 2 * ( w*h + h*d + w*d );
    BodyId id = CreateBody( solver, num_particles );

    const Vector3F begin_pos_ls =
        -Vector3F(
            (f32)(w/2) - ( 1 - ( w % 2 ) ) * 0.5f,
            (f32)(h/2) - ( 1 - ( h % 2 ) ) * 0.5f,
            (f32)(d/2) - ( 1 - ( d % 2 ) ) * 0.5f
            ) * pradius2;

    const f32 particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    Vector3F* pos = MapPosition( solver, id );
    f32* mass_inv = MapMassInv( solver, id );

    array_t< ShapeMatchingCInfo > shape_match_cinfo;
    array::reserve( shape_match_cinfo, num_particles );

   
    u32 pcounter = 0;
    for( u32 iz = 0; iz < d; ++iz )
    {
        for( u32 iy = 0; iy < h; ++iy )
        {
            for( u32 ix = 0; ix < w; ++ix )
            {
                if( shell )
                {
                    if( !EdgeDetect( ix, iy, iz, w, h, d ) )
                        continue;
                }

                const Vector3F pos_ls = begin_pos_ls + Vector3F( (f32)ix, (f32)iy, (f32)iz ) * pradius2;
                const Vector3F pos_ws = ( pose * Point3F( pos_ls ) ).getXYZ();

                pos[pcounter] = pos_ws;
                mass_inv[pcounter] = particle_mass_inv;

                ShapeMatchingCInfo info;
                info.i = pcounter;
                info.rest_pos = pos_ls;
                info.mass = particleMass;
                array::push_back( shape_match_cinfo, info );


                //u32 voxel_index = voxel_grid.index( ix + 1, iy + 1, iz + 1 );
                const u32 voxel_index = pcounter;
                pcounter += 1;
            }
        }
    }

    SYS_ASSERT( pcounter == num_particles );

    Unmap( solver, mass_inv );
    Unmap( solver, pos );

    CalculateLocalPositions( solver, id );
    //SetShapeMatchingConstraints( solver, id, shape_match_cinfo.begin(), shape_match_cinfo.size );
    return id;
}
#endif

static AABBF ComputeAABB( const Vector3F* points, u32 numPoints )
{
    AABBF aabb = AABBF::prepare();
    for( u32 i = 0; i < numPoints; ++i )
        aabb = AABBF::extend( aabb, points[i] );
    return aabb;
}

BodyId CreateFromShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const Vector3F* srcPos, u32 numPositions, const u32* srcIndices, u32 numIndices, float particleMass )
{
    const float jitter = 0.005f;
    bxRandomGen rnd( 0xDEADCEE1 );

    array_t<Vector3F> positions;
    array::reserve( positions, numPositions );
    
    // --- prepare positions
    for( u32 i = 0; i < numPositions; ++i )
        positions[i] = srcPos[i];

    AABBF local_aabb         = ComputeAABB( positions.begin(), numPositions );
    Vector3F local_aabb_size = AABBF::size( local_aabb );
    float max_edge           = maxElem( local_aabb_size );

    // put mesh at the origin and scale to specified size
    const Matrix4F xform = Matrix4F::scale( scale * ( 1.f / max_edge ) ); // prependScale( scale * ( 1.f / max_edge ), Matrix4F::translation( -local_aabb.min ) );
    for( u32 i = 0; i < numPositions; ++i )
        positions[i] = ( xform * Point3F( positions[i] ) ).getXYZ();

    // recompute bounds
    local_aabb      = ComputeAABB( positions.begin(), numPositions );
    local_aabb_size = AABBF::size( local_aabb );
    max_edge        = maxElem( local_aabb_size );
    

    // tweak spacing to avoid edge cases for particles laying on the boundary
    // just covers the case where an edge is a whole multiple of the spacing.
    const float pradius = GetParticleRadius( solver );
    const float spacing = pradius * 2.f;
    const float spacing_eps = spacing * ( 1.0f - 1e-4f );

    // make sure to have at least one particle in each dimension
    int dx, dy, dz;
    dx = spacing > local_aabb_size.x ? 1 : int( local_aabb_size.x / spacing_eps );
    dy = spacing > local_aabb_size.y ? 1 : int( local_aabb_size.y / spacing_eps );
    dz = spacing > local_aabb_size.z ? 1 : int( local_aabb_size.z / spacing_eps );
    int max_dim = maxOfPair( maxOfPair( dx, dy ), dz );
    
    // expand border by two voxels to ensure adequate sampling at edges
    local_aabb.min -= 2.0f*Vector3F( spacing );
    local_aabb.max += 2.0f*Vector3F( spacing );
    max_dim += 4;

    const u32 max_dim_pow3 = max_dim*max_dim*max_dim;

    array_t<u32> voxels;
    array::reserve( voxels, max_dim_pow3 );

    // we shift the voxelization bounds so that the voxel centers
    // lie symmetrically to the center of the object. this reduces the 
    // chance of missing features, and also better aligns the particles
    // with the mesh
    Vector3F meshOffset;
    meshOffset.x = 0.5f * ( spacing - ( local_aabb_size.x - ( dx - 1 )*spacing ) );
    meshOffset.y = 0.5f * ( spacing - ( local_aabb_size.y - ( dy - 1 )*spacing ) );
    meshOffset.z = 0.5f * ( spacing - ( local_aabb_size.z - ( dz - 1 )*spacing ) );
    local_aabb.min -= meshOffset;

    // --- voxelize
    Voxelize( 
        (const float*)positions.begin(), numPositions, (const int*)srcIndices, numIndices, 
        max_dim, max_dim, max_dim, &voxels[0], local_aabb.min, local_aabb.min + Vector3F( max_dim*spacing ) 
        );
    // --- 

    // --- make sdf
    array_t<i32> indices;
    array_t<f32> sdf;
    array::reserve( indices, max_dim_pow3 );
    array::reserve( sdf, max_dim_pow3 );

    MakeSDF( &voxels[0], max_dim, max_dim, max_dim, &sdf[0] );
    // --- 

    u32 num_particles = 0;
    for( u32 i = 0; i < voxels.capacity; ++i )
        num_particles += ( voxels[i] ) ? 1 : 0;

    BodyId id = CreateBody( solver, num_particles );

    const f32 particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    Vector3F* body_pos = MapPosition( solver, id );
    f32*      body_mass_inv = MapMassInv( solver, id );

    array_t<Vector4F> sdf_data;
    
    u32 body_particle_index = 0;
    const Vector3F translation = pose.getTranslation();
    for( int x = 0; x < max_dim; ++x )
    {
        for( int y = 0; y < max_dim; ++y )
        {
            for( int z = 0; z < max_dim; ++z )
            {
                const int index = z*max_dim*max_dim + y*max_dim + x;

                if( !voxels[index] )
                    continue;

                const Vector3F grid_pos = Vector3F( float( x ) + 0.5f, float( y ) + 0.5f, float( z ) + 0.5f );
                const Vector3F jitter_pos = bxRand::unitVector( rnd ) * jitter;
                const Vector3F pos_ls = local_aabb.min + spacing*grid_pos; // +jitter_pos;
                const Vector3F pos_ws = (pose * Point3F(pos_ls)).getXYZ();

                // normalize the sdf value and transform to world scale
                Vector3F sdf_grad;
                SampleSDFGrad( &sdf_grad.x, sdf.begin(), max_dim, x, y, z );
                const Vector3F n = normalizeSafeF( sdf_grad );
                const float d = sdf[index] * max_edge;
                const Vector4F nrm_ls( n, d );

                body_pos[body_particle_index] = pos_ws;
                body_mass_inv[body_particle_index] = particle_mass_inv;
                
                array::push_back( sdf_data, nrm_ls );
                body_particle_index += 1;
            }
        }
    }

    SYS_ASSERT( num_particles == body_particle_index );

    CalculateLocalPositions( solver, id );
    SetSDFData( solver, id, sdf_data.begin(), sdf_data.size );

    Unmap( solver, body_mass_inv );
    Unmap( solver, body_pos );

    return id;
}



BodyId CreateFromPolyShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const bxPolyShape& shape, float particleMass )
{
    const Vector3F* positions = (Vector3F*)shape.positions;
    const u32* indices = shape.indices;
    BodyId id = CreateFromShape( solver, pose, scale, positions, shape.num_vertices, indices, shape.num_indices, particleMass );
    
    return id;
}

BodyId CreateBox( Solver* solver, const Matrix4F& pose, const Vector3F& extents, float particleMass )
{
    bxPolyShape shape;
    bxPolyShape_createBox( &shape, 1 );
    BodyId id = CreateFromPolyShape( solver, pose, extents*2.f, shape, particleMass );
    bxPolyShape_deallocateShape( &shape );
    return id;
}

BodyId CreateSphere( Solver* solver, const Matrix4F& pose, float radius, float particleMass )
{
    bxPolyShape shape;
    bxPolyShape_createShpere( &shape, 8 );
    BodyId id = CreateFromPolyShape( solver, pose, Vector3F(radius*2.f), shape, particleMass );
    bxPolyShape_deallocateShape( &shape );
    return id;
}

}}}///