#include "ship_terrain.h"
#include <resource_manager/resource_manager.h>
#include <math.h>
#include "util/array.h"
#include "rdi/rdi_debug_draw.h"
#include "rdi/rdi_backend.h"
#include "rdi/rdi.h"
#include "../renderer_scene.h"
#include "../renderer_material.h"
#include "util/common.h"

namespace bx{ namespace ship{

    namespace
    {
        static const u32 even_template[6] = { 1, 0, 2, 1, 2, 3 };
        static const u32 odd_template[6] = { 0, 3, 1, 0, 2, 3 };

        void GenerateTileIndices( array_t<u16>& output, u32 numPointsPerTileCol, u32 numPointsPerTileRow )
        {
            u32 quad_counter = 0;
            for( u32 iz = 0; iz < numPointsPerTileRow-1; ++iz )
            {
                const u32 row_offset0 = ( iz + 0 ) * numPointsPerTileCol;
                const u32 row_offset1 = ( iz + 1 ) * numPointsPerTileCol;
                for( u32 ix = 0; ix < numPointsPerTileCol-1; ++ix )
                {
                    const u32 vi[4] =
                    {
                        row_offset0 + ix,
                        row_offset0 + ix + 1,
                        row_offset1 + ix,
                        row_offset1 + ix + 1,
                    };
                    const u32* i_template = ( quad_counter % 2 ) ? odd_template : even_template;

                    for( u32 ii = 0; ii < 6; ++ii )
                    {
                        const u32 vertex_index = vi[i_template[ii]];
                        SYS_ASSERT( vertex_index < 0xFFFF );
                        array::push_back( output, (u16)vertex_index );
                    }

                    ++quad_counter;
                }
            }
        }
    }

    void ComputeTriangleNormal( array_t<float3_t>& normals, const array_t<float3_t>& points, u32 i0, u32 i1, u32 i2 )
    {
        SYS_ASSERT( i0 < array::sizeu( points ) );
        SYS_ASSERT( i1 < array::sizeu( points ) );
        SYS_ASSERT( i2 < array::sizeu( points ) );

        Vector3 p0( xyz_to_m128( points[i0].xyz ) );
        Vector3 p1( xyz_to_m128( points[i1].xyz ) );
        Vector3 p2( xyz_to_m128( points[i2].xyz ) );

        Vector3 n0( xyz_to_m128( normals[i0].xyz ) );
        Vector3 n1( xyz_to_m128( normals[i1].xyz ) );
        Vector3 n2( xyz_to_m128( normals[i2].xyz ) );

        Vector3 v0 = p0 - p1;
        Vector3 v1 = p2 - p1;

        Vector3 n = cross( v1, v0 );
        n0 += n;
        n1 += n;
        n2 += n;

        m128_to_xyz( normals[i0].xyz, n0.get128() );
        m128_to_xyz( normals[i1].xyz, n1.get128() );
        m128_to_xyz( normals[i2].xyz, n2.get128() );
    }

    void GenerateNormals( array_t<float3_t>& normals, const array_t<float3_t>& points, u32 numPointsX, u32 numPointsZ )
    {
        u32 quad_counter = 0;
        for( u32 iz = 0; iz < numPointsZ - 1; ++iz )
        {
            const u32 row_offset0 = iz * numPointsX;
            const u32 row_offset1 = ( iz + 1 ) * numPointsX;
            for( u32 ix = 0; ix < numPointsX - 1; ++ix )
            {
                const u32 vi[4] =
                {
                    row_offset0 + ix,
                    row_offset0 + ix + 1,
                    row_offset1 + ix,
                    row_offset1 + ix + 1,
                };
                const u32* i_template = ( quad_counter % 2 ) ? odd_template : even_template;

                // -- triangle A
                {
                    const u32 i0 = vi[i_template[0]];
                    const u32 i1 = vi[i_template[1]];
                    const u32 i2 = vi[i_template[2]];
                    ComputeTriangleNormal( normals, points, i0, i1, i2 );
                }

                // -- triangle B
                {
                    const u32 i0 = vi[i_template[3]];
                    const u32 i1 = vi[i_template[4]];
                    const u32 i2 = vi[i_template[5]];
                    ComputeTriangleNormal( normals, points, i0, i1, i2 );
                }
                ++quad_counter;
            }
        }
        for( float3_t& n : normals )
        {
            Vector3 tmp( xyz_to_m128( n.xyz ) );
            tmp = normalizeSafe( tmp );
            m128_to_xyz( n.xyz, tmp.get128() );
        }
    }

    void GenerateNormals( array_t<float3_t>& normals, const array_t<float3_t>& points, const array_t<u16>& tileIndices, u32 numTilesX, u32 numTilesZ, u32 numPointsPerTile, u32 numIndicesPerTile )
    {
        for( u32 tz = 0; tz < numTilesZ; ++tz )
        {
            for( u32 tx = 0; tx < numTilesX; ++tx )
            {
                const u32 vertex_offset = ( tz* numTilesX * numPointsPerTile ) + tx* numPointsPerTile;
                for( u32 i = 0; i < numIndicesPerTile; i += 3 )
                {
                    const u32 i0 = tileIndices[i + 0];
                    const u32 i1 = tileIndices[i + 1];
                    const u32 i2 = tileIndices[i + 2];

                    const u32 vi0 = vertex_offset + i0;
                    const u32 vi1 = vertex_offset + i1;
                    const u32 vi2 = vertex_offset + i2;
                    SYS_ASSERT( vi0 < array::sizeu( points ) );
                    SYS_ASSERT( vi1 < array::sizeu( points ) );
                    SYS_ASSERT( vi2 < array::sizeu( points ) );

                    Vector3 p0( xyz_to_m128( points[vi0].xyz ) );
                    Vector3 p1( xyz_to_m128( points[vi1].xyz ) );
                    Vector3 p2( xyz_to_m128( points[vi2].xyz ) );

                    Vector3 n0( xyz_to_m128( normals[vi0].xyz ) );
                    Vector3 n1( xyz_to_m128( normals[vi1].xyz ) );
                    Vector3 n2( xyz_to_m128( normals[vi2].xyz ) );

                    Vector3 v0 = p0 - p1;
                    Vector3 v1 = p2 - p1;

                    Vector3 n = cross( v1, v0 );
                    n0 += n;
                    n1 += n;
                    n2 += n;

                    m128_to_xyz( normals[vi0].xyz, n0.get128() );
                    m128_to_xyz( normals[vi1].xyz, n1.get128() );
                    m128_to_xyz( normals[vi2].xyz, n2.get128() );
                }
            }
        }

        for( float3_t& n : normals )
        {
            Vector3 tmp( xyz_to_m128( n.xyz ) );
            tmp = normalizeSafe( tmp );
            m128_to_xyz( n.xyz, tmp.get128() );
        }
    }

    void Terrain::CreateFromFile( const char* filename, gfx::Scene scene )
    {
        ResourceManager* resource_manager = GResourceManager();
        _file = resource_manager->readFileSync( filename );
        if( !_file.ok() )
        {
            return;
        }

        const u32 num_bytes_per_sample = 4;
        
        _samples = (f32*)_file.bin;
        _num_samples_x = (u32)::sqrt( f32( _file.size / num_bytes_per_sample ) );
        _num_samples_z = _num_samples_x;
        
        const u32 num_samples = _num_samples_x * _num_samples_z;

        const u32 num_segments_per_tile_col = _num_samples_x / _num_tiles_x;
        const u32 num_segments_per_tile_row = _num_samples_z / _num_tiles_z;

        const u32 num_points_per_tile_col = num_segments_per_tile_col + 1;
        const u32 num_points_per_tile_row = num_segments_per_tile_row + 1;
        const u32 num_points_per_tile = num_points_per_tile_col * num_points_per_tile_row;
        const u32 num_indices_per_tile = ( num_points_per_tile_col - 1 )*( num_points_per_tile_row - 1 ) * 6;
        const u32 num_tiles_total = _num_tiles_x * _num_tiles_z;

        array_t<u16> tile_indices;
        array_t<float3_t> linear_points;
        array_t<float3_t> linear_normals;
        array_t<float3_t>& points = _debug_points;
        array_t<float3_t>& normals = _debug_normals;
        array_t<bxAABB> localAABB;

        array::reserve( linear_points, num_samples );
        array::reserve( linear_normals, num_samples );
        array::reserve( points, num_samples );
        array::reserve( normals, num_samples );
        array::reserve( tile_indices, num_indices_per_tile );
        array::reserve( _rsources, num_tiles_total );
        array::reserve( localAABB, num_tiles_total );
        
        GenerateTileIndices( tile_indices, num_points_per_tile_col, num_points_per_tile_row );
        SYS_ASSERT( array::sizeu( tile_indices ) == num_indices_per_tile );

        float min_y = FLT_MAX;
        for( u32 i = 0; i < num_samples; ++i )
            min_y = minOfPair( min_y, _samples[i] );

        const float ext_x = _num_samples_x * 0.5f;
        const float ext_z = _num_samples_z * 0.5f;
        const float ext_y = ::fabsf( min_y ) + 30.f;

        Matrix4 transform = Matrix4::translation( Vector3( -ext_x, -ext_y, -ext_z ) );
        transform = appendScale( transform, Vector3( _sample_scale_xz, _sample_scale_y, _sample_scale_xz ) );

        for( u32 iz = 0; iz < _num_samples_z; ++iz )
        {
            for( u32 ix = 0; ix < _num_samples_x; ++ix )
            {
                const u32 linear_index = iz * _num_samples_x + ix;

                const f32 fx = (f32)ix;
                const f32 fy = ::expf( _samples[linear_index] ) - 1.f;
                const f32 fz = (f32)iz;
                const Vector3 p = mulAsVec4( transform, Vector3( fx, fy, fz ) );
                
                float3_t p3f;
                m128_to_xyz( p3f.xyz, p.get128() );

                array::push_back( linear_points, p3f );
                array::push_back( linear_normals, float3_t( 0.f ) );
            }
        }
        GenerateNormals( linear_normals, linear_points, _num_samples_x, _num_samples_z );

        // -- generate vertex tiles 
        for( u32 tz = 0; tz < _num_tiles_z; ++tz )
        {
            for( u32 tx = 0; tx < _num_tiles_x; ++tx )
            {
                bxAABB aabb = bxAABB::prepare();
                for( u32 iz = 0; iz < num_points_per_tile_row; ++iz )
                {
                    const u32 z_abs = tz * num_segments_per_tile_row + iz;
                    for( u32 ix = 0; ix < num_points_per_tile_col; ++ix )
                    {
                        const u32 x_abs = tx * num_segments_per_tile_col + ix;
                        const u32 linear_index = z_abs * _num_samples_x + x_abs;
                        //SYS_ASSERT( linear_index < _num_samples_x * _num_samples_z );
                        SYS_ASSERT( linear_index < array::sizeu( linear_points ) );

                        const float3_t& point = linear_points[linear_index];
                        array::push_back( points, point );
                        array::push_back( normals, linear_normals[linear_index] );

                        aabb = bxAABB::extend( aabb, Vector3( point.x, point.y, point.z ) );
                    }
                }

                array::push_back( localAABB, aabb );
            }
        }
        //GenerateNormals( normals, points, tile_indices, _num_tiles_x, _num_tiles_z, num_points_per_tile, num_indices_per_tile );
        
        
        rdi::VertexBufferDesc vbuffer_desc_pos = rdi::VertexBufferDesc( rdi::EVertexSlot::POSITION ).DataType( rdi::EDataType::FLOAT, 3 );
        rdi::VertexBufferDesc vbuffer_desc_nrm = rdi::VertexBufferDesc( rdi::EVertexSlot::NORMAL ).DataType( rdi::EDataType::FLOAT, 3 );

        _index_buffer_tile = rdi::device::CreateIndexBuffer( rdi::EDataType::USHORT, num_indices_per_tile, tile_indices.begin() );

        gfx::MaterialHandle hmaterial = gfx::GMaterialManager()->Find( "green" );


        for( u32 tz = 0; tz < _num_tiles_z; ++tz )
        {
            for( u32 tx = 0; tx < _num_tiles_x; ++tx )
            {
                const u32 tile_index = tz * _num_tiles_x + tx;
                const u32 vertex_offset = ( tz* _num_tiles_x * num_points_per_tile ) + tx * num_points_per_tile;

                const float3_t* vertex_data_pos = &points[vertex_offset];
                const float3_t* vertex_data_nrm = &normals[vertex_offset];

                rdi::RenderSourceDesc desc = {};
                desc.Count( num_points_per_tile, num_indices_per_tile );
                desc.VertexBuffer( vbuffer_desc_pos, vertex_data_pos );
                desc.VertexBuffer( vbuffer_desc_nrm, vertex_data_nrm );
                desc.SharedIndexBuffer( _index_buffer_tile );

                rdi::RenderSource rsource = rdi::CreateRenderSource( desc );
                
                char actorName[32];
                sprintf_s( actorName, 32, "terrain_%u_%u", tx, tz );
                gfx::ActorID actor = scene->Add( actorName, 1 );

                scene->SetRenderSource( actor, rsource );
                scene->SetLocalAABB( actor, localAABB[tile_index] );
                scene->SetMatrices( actor, &Matrix4::identity(), 1 );
                scene->SetMaterial( actor, hmaterial );
                
                array::push_back( _rsources, rsource );
                array::push_back( _actors, actor );
            }
        }

    }

    void Terrain::Destroy()
    {
        _file.release();
    }

    void Terrain::DebugDraw( u32 color )
    {
        u32 tile_x = 1;
        u32 tile_z = 1;
        u32 tile_count = 1;

        u32 num_points_per_tile_in_row = _num_samples_x / _num_tiles_x;
        u32 num_points_per_tile_in_col = _num_samples_z / _num_tiles_z;
        u32 num_points_in_tile = num_points_per_tile_in_col * num_points_per_tile_in_row;
        //u32 offset = ( tile_z * _num_samples_z * num_points_per_tile_in_row ) + tile_x * num_points_per_tile_in_col;
        u32 offset = ( tile_z * _num_tiles_x * num_points_in_tile ) + tile_x * num_points_in_tile;
        
        float radius = 0.01f;
        for( u32 i = offset; i < offset + num_points_in_tile*tile_count; ++i )
        {
            const float3_t& pt = _debug_points[i];
            const float3_t& nrm = _debug_normals[i];

            Vector3 point( pt.x, pt.y, pt.z );
            Vector3 normal( nrm.x, nrm.y, nrm.z );
            rdi::debug_draw::AddSphere( Vector4( point, radius ), color, 1 );
            rdi::debug_draw::AddLine( point, point + normal, 0x0000FFFF, 1 );

        }


        //for( float3_t pt : _debug_points )
        //{
        //    

        //}
    }

}
}//