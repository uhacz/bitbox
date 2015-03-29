#pragma once

#include <util/containers.h>

struct bxVoxel_Octree
{
    struct Node
    {
        u8 x, y, z;
        u8 size;
        u32 children[8];
        i32 dataIndex;
    };

    array_t<Node> nodes;
    array_t<size_t> data;
    hashmap_t map;
};

namespace bxVoxel
{
    bxVoxel_Octree* octree_new( int size, bxAllocator* alloc );
    void octree_delete( bxVoxel_Octree** voct, bxAllocator* alloc );
    
    void octree_getShell( array_t<bxVoxel_GpuData>& vxData, const bxVoxel_Octree* voct );
}///
