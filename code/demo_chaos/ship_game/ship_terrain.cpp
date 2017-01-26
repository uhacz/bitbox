#include "ship_terrain.h"
#include <resource_manager/resource_manager.h>
#include <math.h>
#include "util/array.h"

namespace bx{ namespace ship{



    void Terrain::CreateFromFile( const char* filename )
    {
        ResourceManager* resource_manager = GResourceManager();
        _file = resource_manager->readFileSync( filename );
        if( _file.ok() )
        {
            return;
        }

        _samples = (f32*)_file.bin;
        _num_samples_x = (u32)( ::sqrt( (float)_file.size ) ) / sizeof( *_samples );
        _num_samples_z = _num_samples_x;
        const u32 num_samples = _num_samples_x * _num_samples_z;
        const u32 num_points_per_tile = _num_samples_x / _num_tiles_x;
        const u32 num_indices_per_tile = ( num_points_per_tile - 1 )*( num_points_per_tile - 1 ) * 6;

        array_t<u16> tile_indices;
        array_t<float3_t> points;
        array_t<float3_t> normals;
        array::reserve( points, num_samples );
        array::reserve( normals, num_samples );
        
        for( u32 tz = 0; tz < _num_tiles_z; ++tz )
        {
            for( u32 tx = 0; tx < _num_tiles_x; ++tx )
            {
                for( u32 iz = 0; iz < num_points_per_tile; ++iz )
                {
                    const u32 z_abs = tz * num_points_per_tile + iz;
                    for( u32 ix = 0; ix < num_points_per_tile; ++ix )
                    {
                        const u32 x_abs = tx * num_points_per_tile + ix;
                        const u32 linear_index = z_abs * _num_samples_x + x_abs;
                        SYS_ASSERT( linear_index < _num_samples_x * _num_samples_z );

                        const f32 fx = (f32)x_abs;
                        const f32 fy = _samples[linear_index];
                        const f32 fz = (f32)z_abs;

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

}
}//