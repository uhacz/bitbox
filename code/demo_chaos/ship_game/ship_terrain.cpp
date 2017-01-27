#include "ship_terrain.h"
#include <resource_manager/resource_manager.h>
#include <math.h>
#include "util/array.h"
#include "rdi/rdi_debug_draw.h"

namespace bx{ namespace ship{



    void Terrain::CreateFromFile( const char* filename )
    {
        ResourceManager* resource_manager = GResourceManager();
        _file = resource_manager->readFileSync( filename );
        if( !_file.ok() )
        {
            return;
        }

        _samples = (f32*)_file.bin;
        _num_samples_x = (u32)( ::sqrt( (float)_file.size ) ) / sizeof( *_samples );
        _num_samples_z = _num_samples_x;
        const u32 num_samples = _num_samples_x * _num_samples_z;
        const u32 num_points_per_tile_col = _num_samples_x / _num_tiles_x;
        const u32 num_points_per_tile_row = _num_samples_z / _num_tiles_z;
        const u32 num_points_per_tile = num_points_per_tile_col * num_points_per_tile_row;
        const u32 num_indices_per_tile = ( num_points_per_tile_col - 1 )*( num_points_per_tile_row - 1 ) * 6;

        array_t<u16> tile_indices;
        array_t<float3_t>& points = _debug_points;
        array_t<float3_t> normals;
        array::reserve( points, num_samples );
        array::reserve( normals, num_samples );
        
        for( u32 tz = 0; tz < _num_tiles_z; ++tz )
        {
            for( u32 tx = 0; tx < _num_tiles_x; ++tx )
            {
                for( u32 iz = 0; iz < num_points_per_tile_row; ++iz )
                {
                    const u32 z_abs = tz * num_points_per_tile_row + iz;
                    for( u32 ix = 0; ix < num_points_per_tile_col; ++ix )
                    {
                        const u32 x_abs = tx * num_points_per_tile_col + ix;
                        const u32 linear_index = z_abs * _num_samples_x + x_abs;
                        SYS_ASSERT( linear_index < _num_samples_x * _num_samples_z );

                        const f32 fx = (f32)x_abs * 0.1f;
                        const f32 fy = ::pow( _samples[linear_index], 1.f );
                        const f32 fz = (f32)z_abs * 0.1f;

                        array::push_back( points, float3_t( fx, fy, fz ) );
                    }
                }
            }
        }

    }

    void Terrain::Destroy()
    {
        _file.release();
    }

    void Terrain::DebugDraw( u32 color )
    {
        u32 tile_x = 0;
        u32 tile_z = 0;
        u32 tile_count = _num_tiles_x;

        u32 num_points_per_tile_in_row = _num_samples_x / _num_tiles_x;
        u32 num_points_per_tile_in_col = _num_samples_z / _num_tiles_z;
        u32 num_points_in_tile = num_points_per_tile_in_col * num_points_per_tile_in_row;
        //u32 offset = ( tile_z * _num_samples_z * num_points_per_tile_in_row ) + tile_x * num_points_per_tile_in_col;
        u32 offset = ( tile_z * _num_tiles_x * num_points_in_tile ) + tile_x * num_points_in_tile;
        
        float radius = 0.01f;
        for( u32 i = offset; i < offset + num_points_in_tile*tile_count; ++i )
        {
            const float3_t& pt = _debug_points[i];
            Vector3 point( pt.x, pt.y, pt.z );
            rdi::debug_draw::AddSphere( Vector4( point, radius ), color, 1 );
        }


        //for( float3_t pt : _debug_points )
        //{
        //    

        //}
    }

}
}//