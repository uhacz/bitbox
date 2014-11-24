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

struct bxGfxRenderItem_Bucket
{
    u16 index;
    u16 count;
};

struct bxGfxRenderItem
{
    u16 index_renderData;
    u8 mask_render;
    u8 layer;
    bxGfxRenderItem_Bucket bucket_surfaces;
    bxGfxRenderItem_Bucket bucket_instances;
};
namespace bxGfx
{
    inline bxGfxRenderItem_Bucket renderItem_invalidBucket() { bxGfxRenderItem_Bucket b; b.count = 0xFFFF; b.index = 0xFFFF; return b; }
    inline int rebderItem_isValidBucket( const bxGfxRenderItem_Bucket b ) { return b.index != 0xFFFF && b.count != 0xFFFF; }

}///

struct bxGfxRenderList
{
    //// renderData
    bxGdiRenderSource** _rsources;
    bxGfxShadingPass* _shaders;
    bxAABB* _localAABBs;
    
    /// surfaces
    bxGdiRenderSurface* _surfaces;

    //// items
    bxGfxRenderItem* _items;
    
    //// instances
    Matrix4* _worldMatrices;

    i32 _capacity_renderData;
    i32 _capacity_items;
    i32 _capacity_surfaces;
    i32 _capacity_instances;

    i32 _size_renderData;
    i32 _size_items;
    i32 _size_surfaces;
    i32 _size_instances;

    bxGfxRenderList* next;

    int renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB );
    bxGfxRenderItem_Bucket surfacesAdd( const bxGdiRenderSurface* surfaces, int count );
    bxGfxRenderItem_Bucket instancesAdd( const Matrix4* matrices, int count );
    int itemSubmit( int renderDataIndex, bxGfxRenderItem_Bucket surfaces, bxGfxRenderItem_Bucket instances, u8 renderMask = bxGfx::eRENDER_MASK_ALL, u8 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );

    void renderListAppend( bxGfxRenderList* rList );

    bxGfxRenderList();
};
struct bxGfxRenderItem_Iterator
{
    bxGfxRenderItem_Iterator( bxGfxRenderList* rList )
        : _rList( rList )
        , _currentItem( rList->_items )
        , _endItem( rList->_items + rList->_size_items )
    {}

    int ok() const { return _currentItem < _endItem; }
    void next() { ++_currentItem; }

    bxGdiRenderSource* renderSource  ()       { return _rList->_rsources[ _currentItem->index_renderData ]; }
    bxGfxShadingPass   shadingPass   () const { return _rList->_shaders[ _currentItem->index_renderData ]; }
    const bxAABB&      aabb          () const { return _rList->_localAABBs[ _currentItem->index_renderData ]; }
    
    const bxGdiRenderSurface* surfaces() const { return _rList->_surfaces +  _currentItem->bucket_surfaces.index; }
    int nSurfaces                     () const { return _currentItem->bucket_surfaces.count; }
    
    const Matrix4*     worldMatrices () const { return _rList->_worldMatrices + _currentItem->bucket_instances.index; }
    int                nWorldMatrices() const { return _currentItem->bucket_instances.count; }
    
    u8                 renderMask    () const { return _currentItem->mask_render; }
    u8                 renderLayer   () const { return _currentItem->layer; }

    bxGfxRenderList* _rList;
    bxGfxRenderItem* _currentItem;
    bxGfxRenderItem* _endItem;
};


namespace bxGfx
{
    bxGfxRenderList* renderList_new( int maxItems, int maxInstances, bxAllocator* allocator );
    void renderList_delete( bxGfxRenderList** rList, bxAllocator* allocator );
}///

////
////
namespace bxGfx
{
    enum EFramebuffer
    {
        eFRAMEBUFFER_COLOR = 0,
        eFRAMEBUFFER_DEPTH,
        eFRAMEBUFFER_COUNT,
    };

    struct Shared
    {
        struct
        {
            bxGdiShaderFx_Instance* utils;
            bxGdiShaderFx_Instance* texUtils;
        } shader;

        struct
        {
            bxGdiRenderSource* sphere;
            bxGdiRenderSource* box;
        } rsource;
    };

}///


struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    
    void frameBegin( bxGdiContext* ctx );
    void frameDraw( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    void frameEnd( bxGdiContext* ctx );

    
    static bxGfx::Shared* shared() { return &_shared; }

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;
    bxGdiBuffer _cbuffer_shadingData;

    bxGdiTexture _framebuffer[bxGfx::eFRAMEBUFFER_COUNT];

    static bxGfx::Shared _shared;
};
