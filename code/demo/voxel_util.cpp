#include "voxel.h"
#include "voxel_octree.h"

namespace bxVoxel
{
    void util_addBox( bxVoxel_Octree* voct, int w, int h, int d, unsigned colorRGBA )
    {
        //octree_clear( voct );

        for( int iz = 0; iz < d; ++iz )
        {
            for( int iy = 0; iy < h; ++iy )
            {
                for( int ix = 0; ix < d; ++ix )
                {
                    const Vector3 point( (float)ix, (float)iy, float(iz) );
                    octree_insert( voct, point, colorRGBA );
                }
            }
        }
    }
    void util_addSphere( bxVoxel_Octree* voct, int radius, unsigned colorRGBA )
    {
    }
    void util_addPlane( bxVoxel_Octree* voct, int w, int h, unsigned colorRGBA )
    {
    }
}///
