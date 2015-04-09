#pragma once

#include <util/memory.h>

struct bxGdiContext;
struct bxGfxCamera;
struct bxVoxel_Manager;
struct bxVoxel_GfxContext;
struct bxVoxel_GfxDisplayList;

namespace bxVoxel
{
    bxVoxel_GfxContext gfx_contextNew();
    void gfx_contextDelete( bxVoxel_GfxContext** gfx );

    bxVoxel_GfxDisplayList* gfx_displayListNew( int capacity, bxAllocator* alloc = bxDefaultAllocator() );
    void gfx_displayListDelete( bxVoxel_GfxDisplayList** dlist );

    void gfx_displayListBuild( bxVoxel_GfxDisplayList* dlist, bxVoxel_Manager* menago, const bxGfxCamera& camera );
    void gfx_displayListDraw( bxGdiContext* ctx, bxVoxel_GfxDisplayList* dlist, bxVoxel_Manager* menago, const bxGfxCamera& camera );
}///
