#pragma once

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/vectormath/vectormath.h>
#include "gfx_camera.h"

namespace bxGfx
{
    enum ERenderMask
    {
        eRENDER_MASK_COLOR = BIT_OFFSET(1),
        eRENDER_MASK_DEPTH = BIT_OFFSET(2),
        eRENDER_MASK_SHADOW= BIT_OFFSET(3),
        eRENDER_MASK_ALL = eRENDER_MASK_COLOR|eRENDER_MASK_DEPTH|eRENDER_MASK_SHADOW,
    };

    enum ERenderLayer
    {
        eRENDER_LAYER_TOP = 0,
        eRENDER_LAYER_MIDDLE = 127,
        eRENDER_LAYER_BOTTOM = 255,
    };
}///

struct AABB
{
    Vector3 min;
    Vector3 max;
};

struct bxGfxRenderEffect
{
    virtual ~bxGfxRenderEffect() {}
    virtual void draw() = 0;
};

struct bxGfxRenderData
{
    bxGdiRenderSource* rsource;
    bxGdiShaderFx_Instance* fxI;
    bxGdiRenderSurface surface;
    i32 passIndex;
};

struct bxGfxRenderList
{
    struct Item;
    Item* items;
    bxGfxRenderData* renderData;
    Matrix4* worldMatrices;
    AABB* localAABBs;

    i32 capacity;
    i32 size;

    u32 _handle;

    int itemAdd( const bxGfxRenderData& renderData, const Matrix4& worldMatrix, const AABB& localAABB, u16 renderMask = bxGfx::eRENDER_MASK_ALL, u16 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );
};

namespace bxGfx
{
    bxGfxRenderList* renderList_new( int maxItems, bxAllocator* allocator );
    void renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator );
}///


struct bxGfxViews
{
    u16* renderMask;
    bxGfxCamera* camera;
};

struct bxGfxContext
{
    
    u32 renderListSubmit( bxGfxRenderList** rLists, int numLists );

    void frameBegin();
    void frameEnd();


};
