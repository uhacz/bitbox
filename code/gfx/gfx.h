#pragma once

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/vectormath/vectormath.h>

struct bxGfxRenderEffect
{
    virtual ~bxGfxRenderEffect() {}
    virtual void draw() = 0;
};

struct bxGfxRenderListItem
{
    u32 renderListHandle;
    i32 renderDataIndex;
    i32 spatialDataIndex;
    u16 passIndex;
    u8 renderMask;
    u8 renderLayer;
};


struct AABB
{
    Vector3 min;
    Vector3 max;
};

struct bxGfxRenderList
{
    bxGfxRenderListItem* items;
    Matrix4* worldMatrices;
    AABB* localAABBs;

    i32 capacity;
    i32 size;

    u32 handle;
};

struct bxGfxSortItem
{
    u64 hash;
    bxGfxRenderListItem* item;
};

struct bxGfxSortBuffer
{
    
};

struct bxGfxView
{

};

struct bxGfxContext
{
    
};
