#include "voxel.h"
#include "voxel_octree.h"

namespace bxVoxel
{
    void util_addBox( bxVoxel_Map* map, int w, int h, int d, unsigned char colorIndex )
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
                    map_insert( map, point, colorIndex );
                }
            }
        }
    }
    void util_addSphere( bxVoxel_Map* map, int radius, unsigned char colorIndex )
    {
        const float radiusSqr = (float)radius * (float)radius;
        for( int z = -radius; z <= radius; ++z )
        {
            for( int y = -radius; y <= radius; ++y )
            {
                for( int x = -radius; x <= radius; ++x )
                {
                    const Vector3 point( (float)x, (float)y, float(z) );
                    if( lengthSqr( point ).getAsFloat() <= radiusSqr )
                    {
                        map_insert( map, point, colorIndex );
                    }
                }
            }
        }
    }
    void util_addPlane( bxVoxel_Map* map, int w, int h, unsigned char colorIndex )
    {
        const int halfW = w/2;
        const int halfH = h/2;
        for( int z = -halfH; z <= halfH; ++z )
        {
            for( int x = -halfW; x <= halfW; ++x )
            {
                const Vector3 point( (float)x, 0.f, (float)z );
                map_insert( map, point, colorIndex );
            }
        }
    }
}///
