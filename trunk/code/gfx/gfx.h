#pragma once

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/vectormath/vectormath.h>

struct bxGfxContext
{
    
};

struct bxGfxRenderListItem
{
    bxGdiRenderSource* renderSourceIdx;
    bxGdiShaderFx_Instance* shaderInstance;
    u32 spatialDataIdx;
    u16 passIndex;
    u16 renderMask;    
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
};

struct bxGfxSortBuffer
{
    struct Item
    {
        u64 hash;
        bxGfxRenderListItem* item;
    };
};

struct bxGfxView
{};