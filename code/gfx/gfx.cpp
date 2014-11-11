#include "gfx.h"

struct bxGfxRenderList::Item
{
    u32 renderListHandle;
    i32 renderDataIndex;
    i32 spatialDataIndex;
    u16 renderMask;
    u16 renderLayer;
};

union bxGfxSortKey
{
    u64 hash;
    struct
    {
        u64 rSource : 8;
        u64 shader  : 32;
        u64 depth   : 16;
        u64 layer   : 8;
    };
};

struct bxGfxSortItem
{
    u64 hash;
    bxGfxRenderList::Item* item;
};

struct bxGfxSortList
{
    bxGfxSortItem* items;
    i32 capacity;
    i32 size;

    int itemAdd( u64 hash, bxGfxRenderList::Item* item );
};