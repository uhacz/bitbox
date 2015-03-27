#pragma once

#include <util/vectormath/vectormath.h>

struct bxVoxelOctree;
namespace bxVoxel
{
    bxVoxelOctree* octree_new( int size );
    void octree_delete( bxVoxelOctree** voct );
    void octree_insert( bxVoxelOctree* voct, const Vector3& point, size_t data );
    void octree_clear( bxVoxelOctree* voct );

    void octree_debugDraw( bxVoxelOctree* voct );

}///