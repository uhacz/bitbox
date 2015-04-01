#include "voxel.h"
#include "voxel_octree.h"

namespace bxVoxel
{
    void util_addBox( bxVoxel_Octree* voct, int w, int h, int d, unsigned colorRGBA )
    {
        //octree_clear( voct );

        
        const int halfW = w / 2;
        const int halfH = h / 2;
        const int halfD = d / 2;

        for( int iz = -halfD; iz < halfD; ++iz )
        {
            for( int iy = -halfH; iy < halfH; ++iy )
            {
                for( int ix = -halfW; ix < halfW; ++ix )
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
