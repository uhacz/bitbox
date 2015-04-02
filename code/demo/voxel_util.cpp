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
                        octree_insert( voct, point, colorRGBA );
                    }
                }
            }
        }


        //for( int r = radius - 2; r <= radius; ++r )
        //{
        //    for( int z = -radius; z <= radius; ++z )
        //    {
        //        int x = r;
        //        int y = 0;
        //        int radiusError = 1 - x;
        //        while ( x >= y )
        //        {
        //            const float fx = (float)x;
        //            const float fy = (float)y;
        //            const float fz = (float)z;
        //            octree_insert( voct, Vector3(  fx,  fy, fz ), colorRGBA );
        //            octree_insert( voct, Vector3(  fy,  fx, fz ), colorRGBA );
        //            octree_insert( voct, Vector3( -fx,  fy, fz ), colorRGBA );
        //            octree_insert( voct, Vector3( -fy,  fx, fz ), colorRGBA );
        //            octree_insert( voct, Vector3( -fx, -fy, fz ), colorRGBA );
        //            octree_insert( voct, Vector3( -fy, -fx, fz ), colorRGBA );
        //            octree_insert( voct, Vector3(  fx, -fy, fz ), colorRGBA );
        //            octree_insert( voct, Vector3(  fy, -fx, fz ), colorRGBA );
        //            ++y;
        //            if( radiusError < 0 )
        //            {
        //                radiusError += 2*y + 1;
        //            }
        //            else
        //            {
        //                --x;
        //                radiusError += 2*(y - x) + 1;
        //            }
        //        }

        //    }
        //}
    }
    void util_addPlane( bxVoxel_Octree* voct, int w, int h, unsigned colorRGBA )
    {
        const int halfW = w/2;
        const int halfH = h/2;
        for( int z = -halfH; z <= halfH; ++z )
        {
            for( int x = -halfW; x <= halfW; ++x )
            {
                const Vector3 point( (float)x, 0.f, (float)z );
                octree_insert( voct, point, colorRGBA );
            }
        }
    }
}///
