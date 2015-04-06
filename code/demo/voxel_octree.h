#pragma once

#include <util/containers.h>

struct bxVoxel_Octree
{
    struct Node
    {
        float3_t center;
        float3_t size;
        u32 children[8];
        i32 dataIndex;
    };

    array_t<Node> nodes;
    array_t<size_t> data;
};

struct bxVoxel_Map : public hashmap_t
{};

namespace bxVoxel
{
    bxVoxel_Octree* octree_new( bxAllocator* alloc );
    void octree_delete( bxVoxel_Octree** voct, bxAllocator* alloc );
    

    void map_getShell( array_t<bxVoxel_GpuData>& vxData, const bxVoxel_Map& voct );
    int map_getShell( bxVoxel_GpuData* vxData, int vxDataLenght, const bxVoxel_Map& voct );
}///
