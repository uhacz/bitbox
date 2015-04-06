#include "voxel_file.h"
#include "voxel.h"
#include "voxel_octree.h"
#include <resource_manager/resource_manager.h>
#include <util/color.h>
#include <util/debug.h>
#include <MVImporter/mv_vox.h>

namespace bxVoxel
{
    int octree_loadMagicaVox( bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename )
    {
        bxFS::Path absPath = resourceManager->absolutePath( filename );
        MV_Model vox;
        vox.LoadModel( absPath.name );
        
        const int maxSize = maxOfPair( maxOfPair( vox.size.x, vox.size.y ), vox.size.z );
        Matrix4 transform = Matrix4( Matrix3::identity(), Vector3( (f32)maxSize ) * -0.5f  );

        for( size_t ivox = 0; ivox < vox.voxels.size(); ++ivox )
        {
            const MV_Voxel& voxel = vox.voxels[ivox];

            const Vector3 point = mulAsVec4( transform, Vector3( (float)voxel.x, (float)voxel.y, (float)voxel.z ) );
            const MV_RGBA rgba = vox.palette[ voxel.colorIndex ];
            const u32 color = bxColor::toRGBA( rgba.r, rgba.g, rgba.b, rgba.a );
            map_insert( map, point, color );
        }
        
        return 0;
    }

    int octree_loadHeightmapRaw8( bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename )
    {
        bxFS::File file = resourceManager->readFileSync( filename );
        if ( !file.ok() )
            return -1;

        const int size = (int)(sqrt( (float)file.size ));
        SYS_ASSERT( size * size <= file.size );

        const float min = (float)INT8_MIN;
        const float max = (float)INT8_MAX;

        const u8* ysamples = file.bin;
        const float zbegin = maxOfPair( -size * 0.5f, min );
        const float xbegin = zbegin;
        const float yconvert = 1.f / 4.f;
        for( int iz = 0; iz < size; ++iz )
        {
            const float z = minOfPair( zbegin + iz, max );
            for ( int ix = 0; ix < size; ++ix )
            {
                i32 sampleValue = (i32)ysamples[iz * size + ix] - INT8_MAX;
                const float x = minOfPair( xbegin + ix, max );
                
                while( sampleValue >= INT8_MIN )
                {
                    const float y = ( sampleValue ) * yconvert;
                    const Vector3 point( x, y, z );
                    const u32 color = 0x00FF00FF;
                    map_insert( map, point, color );
                    --sampleValue;
                }
            }
        }

        return 0;
    }
}///


