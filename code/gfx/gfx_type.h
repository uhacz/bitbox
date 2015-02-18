#pragma once

#include <util/debug.h>
#include <util/vectormath/vectormath.h>

struct bxGdiShaderFx_Instance;
struct bxGdiRenderSource;
struct bxGfxCamera;

namespace bxGfx
{
    enum ERenderMask
    {
        eRENDER_MASK_COLOR = BIT_OFFSET( 1 ),
        eRENDER_MASK_DEPTH = BIT_OFFSET( 2 ),
        eRENDER_MASK_SHADOW = BIT_OFFSET( 3 ),
        eRENDER_MASK_ALL = eRENDER_MASK_COLOR | eRENDER_MASK_DEPTH | eRENDER_MASK_SHADOW,
    };

    enum ERenderLayer
    {
        eRENDER_LAYER_TOP = 0,
        eRENDER_LAYER_MIDDLE = 127,
        eRENDER_LAYER_BOTTOM = 255,
    };

    struct FrameData
    {
        Matrix4 _camera_view;
        Matrix4 _camera_proj;
        Matrix4 _camera_viewProj;
        Matrix4 _camera_world;
        float4_t _camera_eyePos;
        float4_t _camera_viewDir;
        float4_t _camera_projParams;
        float _camera_fov;
        float _camera_aspect;
        float _camera_zNear;
        float _camera_zFar;
        float _reprojectDepthScale; // (g_zFar - g_zNear) / (-g_zFar * g_zNear)
        float _reprojectDepthBias; // g_zFar / (g_zFar * g_zNear)
        float4_t _renderTarget_rcp_size;
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


namespace bxGfx
{
    enum EShadow
    {
        eSHADOW_NUM_CASCADES = 4,
        eSHADOW_CASCADE_SIZE = 1024*2,
    };
    
    enum EFramebuffer
    {
        eFRAMEBUFFER_COLOR = 0,
        eFRAMEBUFFER_SWAP,
        eFRAMEBUFFER_SHADOWS,
        eFRAMEBUFFER_SHADOWS_VOLUME,
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

    enum ERenderItemMask
    {
        eRENDER_ITEM_USE_STREAMS = BIT_OFFSET(0),
        eRENDER_ITEM_USE_FX = BIT_OFFSET(1),
        eRENDER_ITEM_DEFAULT_MASK = eRENDER_ITEM_USE_STREAMS | eRENDER_ITEM_USE_FX,
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