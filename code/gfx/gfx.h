#pragma once

#include "gfx_render_list.h"
#include "gfx_sort_list.h"
#include "gfx_shadows.h"
#include "gfx_lights.h"
////
////
class bxResourceManager;
struct bxGfxCamera_Params;

struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    
    void bindCamera( bxGdiContext* ctx, const bxGfxCamera& camera );
    
    void frame_begin               ( bxGdiContext* ctx );
    void frame_zPrepass            ( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    void frame_drawShadows         ( bxGdiContext* ctx, bxGfxShadows* shadows, bxGfxRenderList** rLists, int numLists, const bxGfxCamera& camera, const bxGfxLights& lights );
    void frame_drawColor           ( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    void frame_rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera );
    void frame_end                 ( bxGdiContext* ctx );

    bxGdiTexture framebuffer( int index ) { SYS_ASSERT( index < bxGfx::eFRAMEBUFFER_COUNT );  return _framebuffer[index]; }

    int framebufferWidth() const { return _framebuffer->width; }
    int framebufferHeight() const { return _framebuffer->height; }
    
    static bxGfx::Shared* shared() { return &_shared; }
    static void submitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName );
    

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;

    bxGdiTexture _framebuffer[bxGfx::eFRAMEBUFFER_COUNT];

    bxGfxSortList_Color* _sortList_color;
    bxGfxSortList_Depth* _sortList_depth;
    bxGfxSortList_Shadow* _sortList_shadow;

    static bxGfx::Shared _shared;
};



////
////
struct bxGfxLight_Sun;
struct bxGfxPostprocess
{
    void toneMapping( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, float deltaTime );

    void sky( bxGdiContext* ctx, bxGdiTexture outTexture, const bxGfxLight_Sun& sunLight );
    void fog( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, bxGdiTexture depthTexture, const bxGfxLight_Sun& sunLight );

    bxGfxPostprocess();
    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

private:
    void _ShowGUI();

private:

    struct ToneMapping
    {
        u32 currentLuminanceTexture;
        f32 tau;
        f32 autoExposureKeyValue;
        f32 camera_aperture;
        f32 camera_shutterSpeed;
        f32 camera_iso;
        i32 useAutoExposure;

        bxGdiTexture adaptedLuminance[2];
        bxGdiTexture initialLuminance;

        ToneMapping()
            : currentLuminanceTexture(0), tau( 5.f ), autoExposureKeyValue( 0.18f )
            , camera_aperture( 16.f ), camera_shutterSpeed( 1.f / 100.f ), camera_iso(100.f)
            , useAutoExposure(1)
        {}
    } _toneMapping;

    struct Fog
    {
        f32 fallOff;
        f32 fallOffPower;

        Fog()
            : fallOff(0.001f)
        {}
    } _fog;


    bxGdiShaderFx_Instance* _fxI_toneMapping;
    bxGdiShaderFx_Instance* _fxI_fog;
};

