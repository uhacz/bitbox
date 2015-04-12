#pragma once
#include <util/type.h>

struct bxVoxel_Octree;
struct bxVoxel_Map;
struct bxVoxel_Container;

union bxVoxel_ObjectId
{
    u32 hash;
    struct
    {
        u32 index : 16;
        u32 generation : 16;
    };
};