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
        idata->world[index] = world;
        idata->worldIT[index] = transpose( inverse( world ) );
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

struct bxGfxShadingPass
{
    bxGdiShaderFx_Instance* fxI;
    i32 passIndex;
};

struct bxGfxRenderItem
{
    u16 index_renderData;
    u16 index_instances;
    u16 count_instances;
    u8 mask_render;
    u8 layer;
};

struct bxGfxRenderList
{
    bxGdiRenderSource** _rsources;
    bxGfxShadingPass* _shaders;
    bxAABB* _localAABBs;
    
    bxGfxRenderItem* _items;
    Matrix4* _worldMatrices;

    i32 _capacity_renderData;
    i32 _capacity_items;
    i32 _capacity_instances;

    i32 _size_renderData;
    i32 _size_items;
    i32 _size_instances;

    bxGfxRenderList* next;

    int renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB );
    int itemSubmit( int renderDataIndex, const Matrix4* worldMatrices, int nMatrices, u8 renderMask = bxGfx::eRENDER_MASK_ALL, u8 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );

    int renderListAppend( bxGfxRenderList* rList );

    bxGfxRenderList();
};
namespace bxGfx
{
    bxGfxRenderList* renderList_new( int maxItems, int maxInstances, bxAllocator* allocator );
    void renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator );
}///

////
////
struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int startup( bxGdiDeviceBackend* dev );
    void shutdown( bxGdiDeviceBackend* dev );
    
    void frameBegin( bxGdiContext* ctx );
    void frameDraw( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    void frameEnd( bxGdiContext* ctx );

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;
    bxGdiBuffer _cbuffer_shadingData;

    bxGdiTexture _framebuffer_color;
    bxGdiTexture _framebuffer_depth;
};
