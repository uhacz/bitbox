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
    void frameData_fill( FrameData* frameData, const bxGfxCamera& camera, int rtWidth, int rtHeight );

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

union bxGfxRenderItem_Bucket
{
    u32 hash;
    struct
    {
        u16 index;
        u16 count;
    };
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
    inline u32 renderItem_invalidBucket() { return 0xFFFFFFFF; }
    inline int rebderItem_isValidBucket( const bxGfxRenderItem_Bucket b ) { return b.hash != 0xFFFFFFFF; }

}///

////
////
struct BIT_ALIGNMENT_16 bxGfxRenderList
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

    struct
    {
        i32 renderData;
        i32 items;
        i32 surfaces;
        i32 instances;
    } _capacity;
    
    struct
    {
        i32 renderData;
        i32 items;
        i32 surfaces;
        i32 instances;
    } _size;

    bxGfxRenderList* next;

    int renderDataAdd( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB );
    u32 surfacesAdd( const bxGdiRenderSurface* surfaces, int count );
    u32 instancesAdd( const Matrix4* matrices, int count );
    int itemSubmit( int renderDataIndex, u32 surfaces, u32 instances, u8 renderMask = bxGfx::eRENDER_MASK_ALL, u8 renderLayer = bxGfx::eRENDER_LAYER_MIDDLE );

    void renderListAppend( bxGfxRenderList* rList );

    void clear();
    bxGfxRenderList();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxGfxRenderListItemDesc
{
    bxAABB _localAABB;
    bxGdiRenderSource* _rsource;
    bxGdiShaderFx_Instance* _fxI;
    i32 _passIndex;
    i32 _dataIndex;
    
    bxGfxRenderListItemDesc( bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* fxI, int passIndex, const bxAABB& localAABB )
        : _localAABB( localAABB )
        , _rsource( rsource )
        , _fxI( fxI )
        , _passIndex( passIndex )
        , _dataIndex(-1)
    {}

    void setRenderSource( bxGdiRenderSource* rsource )
    {
        _rsource = rsource;
        _dataIndex = -1;
    }
    void setShader( bxGdiShaderFx_Instance* fxI, int passIndex )
    {
        _fxI = fxI;
        _passIndex = passIndex;
        _dataIndex = -1;
    }
    void setLocalAABB( const bxAABB& aabb )
    {
        _localAABB = aabb;
        _dataIndex = -1;
    }
};
namespace bxGfx
{
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, int topology, const Matrix4* matrices, int nMatrices )
    {
        if( itemDesc->_dataIndex < 0 )
        {
            itemDesc->_dataIndex = rList->renderDataAdd( itemDesc->_rsource, itemDesc->_fxI, itemDesc->_passIndex, itemDesc->_localAABB );
        }
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( itemDesc->_rsource, topology );
        u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
        u32 instanceIndex = rList->instancesAdd( matrices, nMatrices );
        rList->itemSubmit( itemDesc->_dataIndex, surfaceIndex, instanceIndex );
    }
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, int topology, const Matrix4& matrix )
    {
        renderList_pushBack( rList, itemDesc, topology, &matrix, 1 );
    }
    inline void renderList_pushBack( bxGfxRenderList* rList, bxGfxRenderListItemDesc* itemDesc, const bxGdiRenderSurface& surf, const Matrix4& matrix )
    {
        if( itemDesc->_dataIndex < 0 )
        {
            itemDesc->_dataIndex = rList->renderDataAdd( itemDesc->_rsource, itemDesc->_fxI, itemDesc->_passIndex, itemDesc->_localAABB );
        }
        u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
        u32 instanceIndex = rList->instancesAdd( &matrix, 1 );
        rList->itemSubmit( itemDesc->_dataIndex, surfaceIndex, instanceIndex );
    }
}///

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxGfxRenderItem_Iterator
{
    bxGfxRenderItem_Iterator( bxGfxRenderList* rList )
        : _rList( rList )
        , _currentItem( rList->_items )
        , _endItem( rList->_items + rList->_size.items )
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

    enum EBindSlot
    {
        eBIND_SLOT_FRAME_DATA = 0,
        eBIND_SLOT_INSTANCE_DATA = 1,
        eBIND_SLOT_LIGHTNING_DATA = 2,

        eBIND_SLOT_LIGHTS_DATA_BUFFER = 0,
        eBIND_SLOT_LIGHTS_TILE_INDICES_BUFFER = 1,
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
            bxGdiRenderSource* fullScreenQuad;
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
    
    void rasterizeFramebuffer( bxGdiContext* ctx, const bxGfxCamera& camera );

    void frameEnd( bxGdiContext* ctx );

    int framebufferWidth() const { return _framebuffer->width; }
    int framebufferHeight() const { return _framebuffer->height; }


    static bxGfx::Shared* shared() { return &_shared; }

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;

    bxGdiTexture _framebuffer[bxGfx::eFRAMEBUFFER_COUNT];

    static bxGfx::Shared _shared;
};
