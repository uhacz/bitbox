#include "flood_fluid.h"
#include <rdi/rdi_debug_draw.h>
#include <util/grid.h>
#include <util/common.h>
#include <util/hashmap.h>

#include "../imgui/imgui.h"

#include "SPHKernels.h"
#include "util/time.h"
#include "util/random.h"
#include "../profiler/Remotery.h"


namespace bx {namespace flood {

    //union Key
    //{
    //    __m128i vec;
    //    struct  
    //    {
    //        i32 x, y, z, w;
    //    };
    //};
    //static Key MakeKey( const __m128 point, const __m128 cellSizeInv )
    //{
    //    static const __m128 _1111 = _mm_set1_ps( 1.f );
    //    static const __m128 _0000 = _mm_set1_ps( 0.f );
    //    __m128 point_in_grid = vec_mul( point, cellSizeInv );
    //    point_in_grid = vec_sel( vec_sub( point_in_grid, _1111 ), point_in_grid, vec_cmpge( point, _0000 ) );
    //    Key k;
    //    k.vec = _mm_cvtps_epi32( point_in_grid );
    //    return k;
    //}
    //static size_t MakeHash( Key key )
    //{
    //    __m128i prime = { 73856093, 19349663, 83492791, 0 };
    //     key.vec = _mm_mul_epi32( prime, key.vec );

    //     return key.x ^ key.y ^ key.z;
    //}
    union PointListCell
    {
        size_t hash;
        struct
        {
            u32 point_index;
            u32 next_cell;
        };

        bool Ok() const { return hash != EMPTY_HASH; }

        static const size_t EMPTY_HASH = UINT64_MAX;
        static const u32    ENDL       = UINT32_MAX;
    };

    void ListPushBack( hashmap_t& map, MapCells& cellAllocator, size_t mapKey, u32 pointIndex )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( map, mapKey );
        if( !cell )
        {
            cell = hashmap::insert( map, mapKey );
            
            PointListCell list_cell = { 0 };
            list_cell.point_index = pointIndex;
            list_cell.next_cell = PointListCell::ENDL;

            u32 begin_list_idnex = array::push_back( cellAllocator, list_cell.hash );
            cell->value = begin_list_idnex;
        }
        else
        {
            SYS_ASSERT( cell->value < (size_t)array::sizeu( cellAllocator ) );
            PointListCell first;
            first.hash = { cellAllocator[(u32)cell->value] };
            
            PointListCell new_first = { 0 };
            new_first.point_index = pointIndex;
            new_first.next_cell = (u32)cell->value;
            
            cell->value = array::push_back( cellAllocator, new_first.hash );
        }
    }
    
    PointListCell ListBegin( hashmap_t& map, size_t mapKey, const MapCells& cellAllocator )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( map, mapKey );
        PointListCell result;
        result.hash = ( cell ) ? cellAllocator[(u32)cell->value] : PointListCell::EMPTY_HASH;
        return result;
    }

    bool ListNext( PointListCell* cell, const MapCells& cellAllocator )
    {
        if( cell->next_cell == PointListCell::ENDL )
        {
            cell->hash = PointListCell::EMPTY_HASH;
            return false;
        }

        SYS_ASSERT( cell->next_cell < cellAllocator.size );

        cell->hash = cellAllocator[cell->next_cell];
        return true;
    }
    
    union SpatialHash
    {
        size_t hash;
        struct
        {
            i64 x : 21;
            i64 y : 21;
            i64 z : 21;
            i64 w : 1;
        };
    };
    static inline SpatialHash MakeHash( const __m128 point, const __m128 cellSizeInv )
    {
        //static const __m128 _1111 = _mm_set1_ps( 1.f );
        //static const __m128 _0000 = _mm_set1_ps( 0.f );
        __m128 point_in_grid = vec_mul( point, cellSizeInv );
        //point_in_grid = vec_sel( vec_sub( point_in_grid, _1111 ), point_in_grid, vec_cmpge( point, _0000 ) );

        const __m128i point_in_grid_int = _mm_cvttps_epi32( point_in_grid );

        SpatialHash shash;
        shash.w = 1;
        shash.x = point_in_grid_int.m128i_i32[0];
        shash.y = point_in_grid_int.m128i_i32[1];
        shash.z = point_in_grid_int.m128i_i32[2];

        return shash;
    }

    static inline SpatialHash MakeHash( const Vector3F& point, const float cellSizeInv )
    {
        const Vector3F point_in_grid = point * cellSizeInv;
        SpatialHash shash;
        shash.w = 1;
        shash.x = (int)point_in_grid.x;
        shash.y = (int)point_in_grid.y;
        shash.z = (int)point_in_grid.z;

        return shash;
    }

    void NeighbourSearch::FindNeighbours( const Vector3F* points, u32 numPoints )
    {
        _num_points = numPoints;
        const __m128 cell_size_inv_vec = _mm_set1_ps( _cell_size_inv );

        array::clear( _point_spatial_hash );
        _point_neighbour_list.clear();
        array::clear( _map_cells );
        hashmap::clear( _map );

        array::reserve( _point_spatial_hash, numPoints );
        _point_neighbour_list.resize( numPoints );
        array::reserve( _map_cells, numPoints );
        hashmap::reserve( _map, numPoints * 2 );

        for( u32 i = 0; i < numPoints; ++i )
        {
            SpatialHash hash = MakeHash( points[i], _cell_size_inv );
            ListPushBack( _map, _map_cells, hash.hash, i );
            array::push_back( _point_spatial_hash, hash.hash );
        }
        
        for( u32 i = 0; i < numPoints; ++i )
        {
            SpatialHash hash = { _point_spatial_hash[i] };
            Indices& indices = _point_neighbour_list[i];
            array::reserve( indices, INITIAL_NEIGHBOUR_COUNT );
            
            for( int dz = -1; dz <= 1; ++dz )
            {
                for( int dy = -1; dy <= 1; ++dy )
                {
                    for( int dx = -1; dx <= 1; ++dx )
                    {
                        SpatialHash neighbour_hash;
                        neighbour_hash.x = hash.x + dx;
                        neighbour_hash.y = hash.y + dy;
                        neighbour_hash.z = hash.z + dz;
                        neighbour_hash.w = 1;

                        PointListCell cell = ListBegin( _map, neighbour_hash.hash, _map_cells );
                        while( cell.Ok() )
                        {
                            if( cell.point_index != i )
                            {
                                array::push_back( indices, cell.point_index );
                            }

                            ListNext( &cell, _map_cells );
                        }

                    }
                }
            }
        }
    }

    void NeighbourSearch::SetCellSize( float value )
    {
        SYS_ASSERT( value > FLT_EPSILON );
        _cell_size_inv = 1.f / value;
    }


    const Indices& NeighbourSearch::GetNeighbours( u32 index ) const
    {
        SYS_ASSERT( index < _point_neighbour_list.size() );
        return _point_neighbour_list[index];
    }

    //////////////////////////////////////////////////////////////////////////
    const NeighbourIndices StaticBody::GetNeighbours( const Vector3F& posWS ) const
    {
        SpatialHash hash = MakeHash( posWS, _map_cell_size_inv );
        const hashmap_t::cell_t* cell = hashmap::lookup( _map, hash.hash );
        if( cell )
        {
            SYS_ASSERT( cell->value < _cell_neighbour_list.size() );
        }

        const Indices* indices = ( cell ) ? &_cell_neighbour_list[cell->value] : nullptr;

        NeighbourIndices result = {};
        if( indices )
        {
            result.data = indices->begin();
            result.size = indices->size;
        }
        return result;
    }


    void StaticBodyCreateBox( StaticBody* body, u32 countX, u32 countY, u32 countZ, float particleRadius, const Matrix4F& toWS )
    {
        body->_particle_radius = particleRadius;
        const float particle_phi = particleRadius * 2.f;
        const Vector3F center_shift = Vector3F( countX*0.5f, countY*0.5f, countZ*0.5f ) * particle_phi;

        const u32 total_count = countX * countY * countZ;
        array::clear( body->_x );
        array::clear( body->_boundary_psi );

        array::reserve( body->_x, total_count );
        array::reserve( body->_boundary_psi, total_count );
        
        for( u32 i = 0; i < total_count; ++i )
            body->_boundary_psi[i] = 0.f;
        
        for( u32 iz = 0; iz < countZ; ++iz )
        {
            for( u32 iy = 0; iy < countY; ++iy )
            {
                for( u32 ix = 0; ix < countX; ++ix )
                {
                    const float x = ix * particle_phi;
                    const float y = iy * particle_phi;
                    const float z = iz * particle_phi;
                    const Vector3F pos_ls = Vector3F( x, y, z ) - center_shift;
                    const Vector3F pos_ws = ( toWS * Point3F( pos_ls ) ).getXYZ();

                    array::push_back( body->_x, pos_ws );
                }
            }
        }
    }

    void StaticBodyCreateBox1( StaticBody* body, float width, float height, float depth, float initialParticleRadius, const Matrix4F& toWS )
    {
        float r = initialParticleRadius;
        float count_xf = ::floor( width  / r );
        float count_yf = ::floor( height / r );
        float count_zf = ::floor( depth  / r );




    }

    void StaticBodyDoNeighbourMap( StaticBody* body, float supportRadius )
    {
        const __m128 cell_size_inv_vec = _mm_set1_ps( 1.f / supportRadius );
        body->_map_cell_size = supportRadius;
        body->_map_cell_size_inv_vec = cell_size_inv_vec;
        body->_map_cell_size_inv = cell_size_inv_vec.m128_f32[0];
        const float cell_size_inv = body->_map_cell_size_inv;

        const u32 num_points = array::sizeu( body->_x );

        hashmap_t tmp_sparse_grid;
        MapCells tmp_sparse_grid_cells;
        //array_t <size_t>  tmp_point_spatial_hash;

        hashmap::reserve( tmp_sparse_grid, iceil( num_points * 3, 2 ) );
        array::reserve( tmp_sparse_grid_cells, num_points );
        //array::reserve( tmp_point_spatial_hash, num_points );

        for( u32 i = 0; i < num_points; ++i )
        {
            SpatialHash hash = MakeHash( body->_x[i], cell_size_inv );
            ListPushBack( tmp_sparse_grid, tmp_sparse_grid_cells, hash.hash, i );
            
            for( int dz = -1; dz <= 1; ++dz )
            {
                for( int dy = -1; dy <= 1; ++dy )
                {
                    for( int dx = -1; dx <= 1; ++dx )
                    {
                        SpatialHash neighbour_hash;
                        neighbour_hash.x = hash.x + dx;
                        neighbour_hash.y = hash.y + dy;
                        neighbour_hash.z = hash.z + dz;
                        neighbour_hash.w = 1;

                        ListPushBack( tmp_sparse_grid, tmp_sparse_grid_cells, neighbour_hash.hash, UINT32_MAX );
                    }
                }
            }

            //array::push_back( tmp_point_spatial_hash, hash.hash );
        }

        body->_cell_neighbour_list.resize( hashmap::size( tmp_sparse_grid ) );

        u32 counter = 0;
        hashmap::iterator map_it( tmp_sparse_grid );
        while( map_it.next() )
        {
            hashmap_t::cell_t* cell = *map_it;
            SpatialHash hash = { cell->key };
            Indices& indices = body->_cell_neighbour_list[counter];

            {
                SYS_ASSERT( hashmap::lookup( body->_map, cell->key ) == nullptr );
                hashmap_t::cell_t* final_cell = hashmap::insert( body->_map, cell->key );
                final_cell->value = (size_t)counter;
            }
            
            for( int dz = -1; dz <= 1; ++dz )
            {
                for( int dy = -1; dy <= 1; ++dy )
                {
                    for( int dx = -1; dx <= 1; ++dx )
                    {
                        SpatialHash neighbour_hash;
                        neighbour_hash.x = hash.x + dx;
                        neighbour_hash.y = hash.y + dy;
                        neighbour_hash.z = hash.z + dz;
                        neighbour_hash.w = 1;

                        PointListCell cell = ListBegin( tmp_sparse_grid, neighbour_hash.hash, tmp_sparse_grid_cells );
                        while( cell.Ok() )
                        {
                            if( cell.point_index != UINT32_MAX )
                            {
                                array::push_back( indices, cell.point_index );
                            }
                            ListNext( &cell, tmp_sparse_grid_cells );
                        }
                    }
                }
            }
            
            ++counter;
        }

        const u32 hashmapSize = num_points * 2;
        hash_grid::Build( &body->_hash_grid, body->_x.begin(), body->_x.size, hashmapSize, supportRadius );
    }

    void StaticBodyComputeBoundaryPsi( StaticBody* sbody, float density0 )
    {
        for( u32 i = 0; i < sbody->_x.size; ++i )
        {
            float psi = 0.f;
            float delta = PBD::Poly6Kernel::W_zero();
            const Vector3F& xi = sbody->_x[i];

            const NeighbourIndices& nindices = sbody->GetNeighbours( sbody->_x[i] );
            if( nindices.Ok() )
            {
                for( u32 nj = 0; nj < nindices.size; ++nj )
                {
                    const u32 j = nindices.data[nj];
                    if( j == i )
                        continue;

                    const Vector3F& xj = sbody->GetPosition( j );

                    delta += PBD::Poly6Kernel::W( xi - xj );
                }

                float volume = 1.f / delta;
                psi = density0 * volume;
            }

            sbody->_boundary_psi[i] = psi;
        }
    }

    void StaticBodyDebugDraw( const StaticBody& body, u32 color )
    {
        const u32 max_visible_particles = 100;

        bxRandomGen rnd( (u32)bxTime::ms() );
        for( u32 i = 0; i < max_visible_particles; ++i )
        {
            u32 index = rnd.get0n( body._x.size );

            const Vector3 v3( xyz_to_m128( &body._x[index].x ) );

            //rdi::debug_draw::AddSphere( Vector4( v3, body._particle_radius ), color, 1 );
            rdi::debug_draw::AddBox( Matrix4::translation( v3 ), Vector3( body._particle_radius ), color, 1 );
        }
    }



    //////////////////////////////////////////////////////////////////////////
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
    f->support_radius = 4.f * particleRadius;
    PBD::CubicKernel::setRadius( f->support_radius );
    PBD::Poly6Kernel::setRadius( f->support_radius );
    PBD::SpikyKernel::setRadius( f->support_radius );

    f->_neighbours.SetCellSize( f->particle_radius * 2 );

    for( u32 i = 0; i < numParticles; ++i )
    {
        array::push_back( f->x, Vector3F( 0.f ) );
        array::push_back( f->p, Vector3F( 0.f ) );
        array::push_back( f->v, Vector3F( 0.f ) );
        array::push_back( f->density, 0.f );
        array::push_back( f->lambda, 0.f );
        array::push_back( f->dpos, Vector3F( 0.f ) );
    }
}

void FluidInitMass( Fluid* f )
{
    // each particle represents a cube with a side length of r		
    // mass is slightly reduced to prevent pressure at the beginning of the simulation
    const float diam = 2.0f * f->particle_radius;
    const float V = diam * diam * diam;
    f->particle_mass = V * f->density0 ;
    //f->particle_mass = 1.f;
}
void FluidInitBox( Fluid* f, u32 width, u32 height, u32 depth, const Matrix4F& pose )
{
    //float fa = ::pow( (float)f->NumParticles(), 1.f / 3.f );
    //u32 a = (u32)fa;

    Vector3F offset( (f32)width, (f32)height, (f32)depth );
    offset *= -0.5f;

    Vector3F spacing( f->particle_radius * 3.f );

    const bxGrid grid( width, height, depth );

    for( u32 z = 0; z < depth; ++z )
    {
        for( u32 y = 0; y < height; ++y )
        {
            for( u32 x = 0; x < width; ++x )
            {
                const Vector3F xyz = mulPerElem( Vector3F( (f32)x, (f32)y, (f32)z ) + offset, spacing );
                const Vector3F pos = (pose * Point3F( xyz )).getXYZ();

                const u32 index = grid.index( x, y, z );
                SYS_ASSERT( index < f->NumParticles() );

                f->x[index] = pos;
                f->p[index] = pos;
                f->v[index] = Vector3F( 0.f );
            }
        }
    }
}

void FluidCreateBox( Fluid* f, u32 width, u32 height, u32 depth, float particleRadius, const Matrix4F& pose )
{
    const u32 num_particles = width * height * depth;
    FluidCreate( f, num_particles, particleRadius );
    FluidInitBox( f, width, height, depth, pose );
    FluidInitMass( f );
}

static Fluid* g_fluid = nullptr;
static float max_lambda = 0.f;
static int debug_i = 0;

void FluidSolvePressure1( Fluid* f, const FluidColliders& colliders )
{
    const u32 solver_iterations = 8;

    const float eps = 1.0e-8f;
    const float density0 = f->density0;
    const float density0_inv = 1.f / density0;
    const float pmass = f->particle_mass;
    const float pmass_div_density0 = pmass / density0;

    const u32 num_points = array::sizeu( f->p );

    for( u32 sit = 0; sit < solver_iterations; ++sit )
    {
        for( u32 i = 0; i < num_points; ++i )
        {
            const Vector3F& xi = f->p[i];

            float density = pmass * PBD::Poly6Kernel::W_zero();
            float grad_pj_norm_sqr = 0.f;
            Vector3F grad_pk_self( 0.f );

            const Indices& neighbour_indices = f->_neighbours.GetNeighbours( i );
            const u32 num_neighbours = array::sizeu( neighbour_indices );
            for( u32 nj = 0; nj < num_neighbours; ++nj )
            {
                const u32 j = neighbour_indices[nj];
                const Vector3F& xj = f->p[j];
                const Vector3F xi_xj = xi - xj;

                density += pmass * PBD::Poly6Kernel::W( xi_xj );
                
                const Vector3F grad = -pmass_div_density0 * PBD::SpikyKernel::gradW( xi_xj );
                grad_pk_self -= grad;
                grad_pj_norm_sqr += lengthSqr( grad );
            }

            for( u32 ibody = 0; ibody < colliders.num_static_bodies; ++ibody )
            {
                const StaticBody& sbody = colliders.static_bodies[ibody];
                const NeighbourIndices neighbour_indices = sbody.GetNeighbours( xi );
                if( neighbour_indices.Ok() )
                {
                    for( u32 nj = 0; nj < neighbour_indices.size; ++nj )
                    {
                        const u32 j= neighbour_indices.data[nj];
                        const Vector3F& xj = sbody.GetPosition( j );
                        const Vector3F xi_xj = xi - xj;
                        const float boundary_psi = sbody.GetBoundaryPsi( j );

                        density += boundary_psi * PBD::Poly6Kernel::W( xi_xj );

                        const Vector3F grad = -boundary_psi * density0_inv * PBD::SpikyKernel::gradW( xi_xj );
                        grad_pk_self -= grad;
                        grad_pj_norm_sqr += lengthSqr( grad );

                        if( i == debug_i )
                        {
                            Matrix4 pose( Matrix3::identity(), Vector3( xyz_to_m128( &xj.x ) ) );
                            rdi::debug_draw::AddBox( pose, Vector3( f->particle_radius ), 0x0000FFFF, 1 );
                        }

                    }
                }

                //{
                //    const HashGridStatic::Indices indices = sbody._hash_grid.Lookup( xi );
                //    for( u32 jj = 0; jj < indices.count; ++jj )
                //    {
                //        const u32 j = indices.data[jj];
                //        const Vector3F& xj = sbody.GetPosition( j );
                //        if( i == debug_i )
                //        {
                //            Matrix4 pose( Matrix3::identity(), Vector3( xyz_to_m128( &xj.x ) ) );
                //            rdi::debug_draw::AddBox( pose, Vector3( f->particle_radius ), 0x0000FFFF, 1 );
                //        }

                //    }
                //}
            }


            grad_pj_norm_sqr += lengthSqr( grad_pk_self );
            
            const float C = maxOfPair( ( density / density0 ) - 1.f, 0.f );
            //const float C = ( ( density / density0 ) - 1.f );
            f->density[i] = density;
            f->lambda[i] = -C / ( grad_pj_norm_sqr + eps );
            if( i == debug_i )
            {
                max_lambda = maxOfPair( max_lambda, ::fabs( f->lambda[i] ) );
            }
        }
        


        const float scorr_k = 0.01f;
        //const float scorr_dq = 0.1f * f->support_radius;
        const float scorr_denom = 1.f / PBD::Poly6Kernel::W_zero();
        
        
        for( u32 i = 0; i < num_points; ++i )
        {

            const Vector3F& xi = f->p[i];
            const float lambda_i = f->lambda[i];

            Vector3F dpos( 0.f );

            //if( lambda_i != 0.f )
            {

                const Indices& neighbour_indices = f->_neighbours.GetNeighbours( i );
                const u32 num_neighbours = array::sizeu( neighbour_indices );
                for( u32 nj = 0; nj < num_neighbours; ++nj )
                {
                    const u32 j = neighbour_indices[nj];
                    const Vector3F& xj = f->p[j];
                    const float lambda_j = f->lambda[j];
                    const float lambda_sum = lambda_j + lambda_i;
                    const Vector3F xi_xj = xi - xj;

                    const float scorr_e = PBD::Poly6Kernel::W( xi_xj ) * scorr_denom;
                    const float scorr_e2 = scorr_e * scorr_e;
                    const float scorr_e4 = scorr_e2 * scorr_e2;
                    const float scorr = -scorr_k * scorr_e4;

                    const Vector3F grad = pmass_div_density0 * PBD::SpikyKernel::gradW( xi_xj );
                    dpos += ( lambda_sum + scorr ) * grad;
                }

                for( u32 ibody = 0; ibody < colliders.num_static_bodies; ++ibody )
                {
                    const StaticBody& sbody = colliders.static_bodies[ibody];
                    const NeighbourIndices neighbour_indices = sbody.GetNeighbours( xi );
                    if( neighbour_indices.Ok() )
                    {
                        for( u32 nj = 0; nj < neighbour_indices.size; ++nj )
                        {
                            const u32 j = neighbour_indices.data[nj];
                            const Vector3F& xj = sbody.GetPosition( j );
                            const Vector3F xi_xj = xi - xj;

                            const float scorr_e = PBD::Poly6Kernel::W( xi_xj ) * scorr_denom;
                            const float scorr_e2 = scorr_e * scorr_e;
                            const float scorr_e4 = scorr_e2 * scorr_e2;
                            const float scorr = -scorr_k * scorr_e4;

                            const Vector3F grad = sbody.GetBoundaryPsi( j ) * density0_inv * PBD::SpikyKernel::gradW( xi_xj );
                            const Vector3F dx = ( lambda_i + scorr ) * grad;
                            dpos += dx;
                        }
                    }
                }
            }

            f->dpos[i] = dpos;
        }
        
        for( u32 i = 0; i < num_points; ++i )
        {
            f->p[i] += f->dpos[i];

            Vector3F xi = f->p[i];

            for( u32 cj = 0; cj < colliders.num_planes; ++cj )
            {
                const Vector4F& plane = colliders.planes[cj];
                const float d = dot( plane, Vector4F( xi, 1.f ) ) - f->particle_radius;
                Vector3F dpos = -plane.getXYZ() * minOfPair( d, 0.f );   
                xi += dpos;
            }

            f->p[i] = xi;
        }
        //if( sit >= 2 )
        //{
        //    if( avg_density_err <= eta )
        //        break;
        //}
    }
}


void FluidSolvePressure( Fluid* f, const FluidColliders& colliders )
{
    g_fluid = f;
    const u32 num_particles = f->NumParticles();
    const float num_particles_inv = 1.f / num_particles;
    
    const float eps = 1.0e-6f;
    const float particle_mass = f->particle_mass;
    const float density0 = f->density0;
    const float density0_inv( 1.f / density0 );
    const float particle_mass_vec( particle_mass );
    const float particle_mass_div_density0 = particle_mass_vec * density0_inv;
    const float support_radius_vec( f->support_radius );

    const Vector3F* x = array::begin( f->p );

    const float eta = f->_maxError * 0.01f * density0;
    u32 iterations = 0;
    float avg_density_err = 0.f;
    
    //const float aa = f->support_radius * f->support_radius;
    //const float s = aa * PI / (f->particle_radius*f->particle_radius);
    while( ( ( avg_density_err > eta ) || ( iterations < 2 ) ) && ( iterations < f->_maxIterations ) )
    {
        avg_density_err = 0.f;

        //memset( f->density.begin(), 0x00, num_particles * sizeof( f32 ) );

        for( u32 i = 0; i < num_particles; ++i )
        {
            float density = particle_mass * PBD::Poly6Kernel::W_zero();
            const Indices& neighbours = f->_neighbours.GetNeighbours( i );

            for( u32 ni = 0; ni < neighbours.size; ++ni )
            {
                const u32 j = neighbours[ni];
                float value = particle_mass * PBD::Poly6Kernel::W( x[i] - x[j] );
                density += value;
            }

            for( u32 ibody = 0; ibody < colliders.num_static_bodies; ++ibody )
            {
                const StaticBody& sbody = colliders.static_bodies[ibody];
                const NeighbourIndices neighbour_indices = sbody.GetNeighbours( x[i] );
                if( neighbour_indices.Ok() )
                {
                    for( u32 j = 0; j < neighbour_indices.size; ++j )
                    {
                        const u32 j_index = neighbour_indices.data[j];
                        const Vector3F& xj = sbody.GetPosition( j_index );
                        density += particle_mass * PBD::Poly6Kernel::W( x[i] - xj );
                    }
                }
            }

            
            f->density[i] = density;
            avg_density_err += ( maxOfPair( density, density0 ) - density0 ) * num_particles_inv;
        
            // -- compute lambda
            // Evaluate constraint function
            const float C = maxOfPair( density / density0 - 1.0f, 0.0f );			// clamp to prevent particle clumping at surface


            if( C != 0.0f )
            {
                // Compute gradients dC/dx_j 
                float sum_grad_C2 = 0.f;
                Vector3F gradC_i( 0.0f, 0.0f, 0.0f );

                const Indices& neighbours = f->_neighbours.GetNeighbours( i );
                for( u32 ni = 0; ni < neighbours.size; ++ni )
                {
                    const u32 j = neighbours[ni];

                    const Vector3F gradC_j = -(particle_mass_div_density0) * PBD::SpikyKernel::gradW( x[i] - x[j] );
                    sum_grad_C2 += smath::lengthSqr( gradC_j );
                    gradC_i -= gradC_j;
                }

                for( u32 ibody = 0; ibody < colliders.num_static_bodies; ++ibody )
                { // boundary
                    const StaticBody& sbody = colliders.static_bodies[ibody];
                    const NeighbourIndices neighbour_indices = sbody.GetNeighbours( x[i] );
                    if( neighbour_indices.Ok() )
                    {
                        for( u32 j = 0; j < neighbour_indices.size; ++j )
                        {
                            const u32 j_index = neighbour_indices.data[j];
                            const Vector3F& xj = sbody.GetPosition( j_index );
                            
                            const Vector3F gradC_j = -(particle_mass_div_density0) * PBD::SpikyKernel::gradW( x[i] - xj );
                            sum_grad_C2 += lengthSqr( gradC_j );
                            gradC_i -= gradC_j;
                        }
                    }
                }

                // Compute lambda
                sum_grad_C2 += lengthSqr( gradC_i );
                f->lambda[i] = -C / ( sum_grad_C2 + eps );

                if( i == debug_i )
                {
                    max_lambda = maxOfPair( max_lambda, ::fabs( f->lambda[i] ) );
                }
            }
            else
            {
                f->lambda[i] = 0.f;
            }
        }

        // -- position correction
        for( u32 i = 0; i < num_particles; ++i )
        {
            Vector3F corr( 0.f );

            const Vector3F& xi = f->p[i];

            const Indices& neighbours = f->_neighbours.GetNeighbours( i );
            for( u32 ni = 0; ni < neighbours.size; ++ni )
            {
                const u32 j = neighbours[ni];
                const Vector3F& xj = f->p[j];

                const Vector3F gradC_j = -(particle_mass_div_density0) * PBD::SpikyKernel::gradW( xi - xj );
                corr -= ( f->lambda[i] + f->lambda[j] ) * gradC_j;
            }
            
            for( u32 ibody = 0; ibody < colliders.num_static_bodies; ++ibody )
            {
                const StaticBody& sbody = colliders.static_bodies[ibody];
                const NeighbourIndices neighbour_indices = sbody.GetNeighbours( x[i] );
                if( neighbour_indices.Ok() )
                {
                    for( u32 j = 0; j < neighbour_indices.size; ++j )
                    {
                        const u32 j_index = neighbour_indices.data[j];
                        const Vector3F& xj = sbody.GetPosition( j_index );

                        const Vector3F gradC_j = -(particle_mass_div_density0)* PBD::SpikyKernel::gradW( xi - xj );
                        const Vector3F dx = 2.0f * f->lambda[i] * gradC_j;
                        corr -= dx;

                        //m_model->getForce( particleId.point_set_id, neighborIndex ) += m_model->getMass( i ) * dx * invH2;
                    }
                }
            }
            f->dpos[i] = corr; // +corr_colliders;
        }

        for( u32 i = 0; i < num_particles; ++i )
        {
            f->p[i] += f->dpos[i];

            Vector3F xi = f->p[i];

            for( u32 cj = 0; cj < colliders.num_planes; ++cj )
            {
                const Vector4F& plane = colliders.planes[cj];
                const float d = dot( plane, Vector4F( xi, 1.f ) );
                Vector3F dpos = -plane.getXYZ() * minOfPair( d, 0.f );
                xi += dpos;
            }

            f->p[i] = xi;
        }

        ++iterations;
    }
}

void FluidTick( Fluid* f, const FluidSimulationParams& params, const FluidColliders& colliders, float deltaTime )
{
    const float fluid_delta_time = 0.005f;
    const float fluid_delta_time_inv = 1.f / fluid_delta_time;
    const u32 max_iterations = 8;
    f->_dt_acc += deltaTime;

    const u32 n = f->NumParticles();
    const float pmass_inv = 1.f / f->particle_mass;

    u32 iteration = 0;
#if 1 
    while( f->_dt_acc >= fluid_delta_time )
    {
        for( u32 i = 0; i < n; ++i )
        {
            Vector3F v = f->v[i] + params.gravity * fluid_delta_time;
            Vector3F p = f->x[i] + v*fluid_delta_time;

            f->v[i] = v;
            f->p[i] = p;
        }

        f->_neighbours.FindNeighbours( array::begin( f->p ), array::sizeu( f->p ) );

        {
            FluidSolvePressure1( f, colliders );
        }

        for( u32 i = 0; i < n; ++i )
        {
            f->v[i] = ( f->p[i] - f->x[i] ) * fluid_delta_time_inv;
            f->x[i] = f->p[i];
        }

        f->_dt_acc -= fluid_delta_time;

        if( ++iteration >= max_iterations )
        {
            f->_dt_acc = 0.f;
            break;
        }

    }
#endif
    //deltaTime = 0.005f;
    
    


    
    {
        if( ImGui::Begin( "FluidDebug" ) )
        {
            ImGui::InputInt( "particle index", &debug_i );
            ImGui::Text( "density (%i): %f", debug_i, f->density[debug_i] );
            ImGui::Text( "lambda(%i): %f", debug_i, max_lambda );

            ImGui::Checkbox( "show density", &f->_debug.show_density );
        }
        ImGui::End();

        if( iteration )
        {
            const Vector3 box_ext( f->particle_radius );
            rdi::debug_draw::AddBox( Matrix4::translation( Vector3( xyz_to_m128( &f->x[debug_i].x ) ) ), box_ext, 0x00FF00FF, 1 );
            const Indices& neighbours = f->_neighbours.GetNeighbours( debug_i );
            if( neighbours.size )
            {
                for( u32 j : neighbours )
                {
                    rdi::debug_draw::AddBox( Matrix4::translation( Vector3( xyz_to_m128( &f->x[j].x ) ) ), box_ext, 0xFF0000FF, 1 );
                }
            }
        }
    }


    for( u32 i = 0; i < f->x.size; ++i )
    {
        //rdi::debug_draw::AddBox( Matrix4::translation( pos ), Vector3( f->particle_radius ), 0x0000FFFF, 1 );
        const Vector3F& pos = f->x[i];
        const Vector3 v3( xyz_to_m128( &pos.x ) );

        u32 color = 0x0000FFFF;
        if( f->_debug.show_density )
        {
            const Vector3F zero_color( 1.f, 1.f, 1.f );
            const Vector3F one_color( 1.f, 0.f, 0.f );

            const float density = f->density[i];
            const float alpha = density / f->density0;

            const Vector3F color3f = lerp( alpha, zero_color, one_color );

            const float scaler = 255.f;
            u8 r = (u8)( color3f.x * scaler );
            u8 g = (u8)( color3f.y * scaler );
            u8 b = (u8)( color3f.z * scaler );

            color = ( r << 24 ) | ( g << 16 ) | ( b << 8 ) | 0xFF;
        }

        //rdi::debug_draw::AddSphere( Vector4( v3, f->particle_radius ), color, 1 );
        rdi::debug_draw::AddBox( Matrix4::translation( v3 ), Vector3( f->particle_radius ), color, 1 );
    }
}



}}///