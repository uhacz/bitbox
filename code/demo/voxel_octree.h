#pragma once

#include <util/containers.h>

struct bxAABB;

struct bxVoxel_Octree
{
    struct BIT_ALIGNMENT_16 Node
    {
        Vector4 size;
        Vector3 center;
        u32 children[8];
        i32 dataIndex;
    };

    array_t<Node> nodes;
    array_t<size_t> data;
};

struct bxVoxel_Map : public hashmap_t
{};

struct bxVoxel_GpuData
{
    i8 x;
    i8 y;
    i8 z;
    u8 colorIndex;
    //u32 gridIndex;
    //u32 colorRGBA;
};

namespace bxVoxel
{
    bxVoxel_Octree* octree_new( bxAllocator* alloc );
    void octree_delete( bxVoxel_Octree** voct, bxAllocator* alloc );
    void octree_init( bxVoxel_Octree* voct, const Vector3& center, const Vector3& size );
    
    void map_getShell( array_t<bxVoxel_GpuData>& vxData, const bxVoxel_Map& voct );
    int map_getShell( bxVoxel_GpuData* vxData, int vxDataLenght, const bxVoxel_Map& voct, bxAABB* bbox );
}///
