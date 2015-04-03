#include "voxel_file.h"
#include "voxel.h"
#include "voxel_octree.h"
#include <resource_manager/resource_manager.h>
#include <util/color.h>
#include <MVImporter/mv_vox.h>
#include <MVImporter/mv_slab.h>

namespace bxVoxel
{
    int octree_loadMagicaVox( bxResourceManager* resourceManager, bxVoxel_Octree* voct, const char* filename )
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
            octree_insert( voct, point, color );
        }
        
        return 0;
    }
}///


