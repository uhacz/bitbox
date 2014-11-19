#pragma once

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/bbox.h>
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

    struct FrameData
    {
        Matrix4 viewMatrix;
        Matrix4 projMatrix;
        Matrix4 viewProjMatrix;
        Matrix4 cameraWorldMatrix;
        Vector4 cameraParams;
        Vector4 projParams;
        Vector4 renderTargetSizeRcp;
        Vector4 eyePos;
        Vector4 viewDir;
    };
    void frameData_fill( FrameData* frameData, const bxGfxCamera& camera );

    static const int cMAX_WORLD_MATRICES = 16;
    struct InstanceData
    {
        Matrix4 world[cMAX_WORLD_MATRICES];
        Matrix4 worldIT[cMAX_WORLD_MATRICES];
    };
    inline int instanceData_setMatrix( InstanceData* idata, int index, const Matrix4& world )
    {
        SYS_ASSERT( index < cMAX_WORLD_MATRICES );
        idata->world = world;
        idata->worldIT = transpose( inverse( world ) );
        return index + 1;
    }

}///

////
////
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
    bxGfxRenderData* renderData;
    Matrix4* worldMatrices;
    bxAABB* localAABBs;

    i32 capacity;
    i32 size;

    bxGfxRenderList* next;

    int itemAdd( const bxGfxRenderData& renderData, const Matrix4& worldMatrix, const bxAABB& localAABB, u16 renderMask = bxGfx::eRENDER_MASK_ALL, u16 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );
    int renderListAdd( bxGfxRenderList* rList );
};
namespace bxGfx
{
    bxGfxRenderList* renderList_new( int maxItems, bxAllocator* allocator );
    void renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator );
}///

////
////
struct bxGfxView
{
    bxGfxCamera* camera;
    bxGdiTexture output;
    u16 renderMask;
};

////
////
struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int startup( bxGdiDeviceBackend* dev );
    void shutdown( bxGdiDeviceBackend* dev );
    
    void renderListSubmit( bxGfxRenderList** rLists, int numLists );
    void viewSubmit( bxGfxCamera* camera, unsigned renderMask, bxGdiTexture outputTexture );

    void frameBegin( bxGdiContext* ctx );
    void frameDraw( bxGdiContext* ctx );
    void frameEnd( bxGdiContext* ctx );

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;
    bxGdiBuffer _cbuffer_shadingData;

    bxGdiTexture _framebuffer_color;
    bxGdiTexture _framebuffer_depth;

    array_t< bxGfxRenderList* > _renderLists;
    array_t< bxGfxView > _views;
};
