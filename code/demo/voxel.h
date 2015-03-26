#pragma once

#include <util/vectormath/vectormath.h>

struct bxVoxelOctree;
namespace bxVoxel
{
    bxVoxelOctree* octree_new( int size );
    void octree_delete( bxVoxelOctree** voct );
    unsigned octree_insert( bxVoxelOctree* voct, const Vector3& point );
}///