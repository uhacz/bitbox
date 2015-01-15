#pragma once

#include "gfx_render_list.h"

////
////
class bxResourceManager;

struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    
    void frameBegin( bxGdiContext* ctx );
    void frameDraw( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    
    void rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera );

    void frameEnd( bxGdiContext* ctx );

    bxGdiTexture framebuffer( int index ) { SYS_ASSERT( index < bxGfx::eFRAMEBUFFER_COUNT );  return _framebuffer[index]; }

    int framebufferWidth() const { return _framebuffer->width; }
    int framebufferHeight() const { return _framebuffer->height; }


    static bxGfx::Shared* shared() { return &_shared; }
    static void submitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName );

private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;

    bxGdiTexture _framebuffer[bxGfx::eFRAMEBUFFER_COUNT];

    static bxGfx::Shared _shared;
};

////
////

struct bxGfxPostprocess
{
    void toneMapping( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, int fbWidth, int fbHeight, float deltaTime );

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

        bxGdiTexture adaptedLuminance[2];
        bxGdiTexture initialLuminance;

        ToneMapping()
            : currentLuminanceTexture(0), tau( 1.25f ), autoExposureKeyValue( 0.18f )
            , camera_aperture( 16.f ), camera_shutterSpeed( 1.f/100.f ), camera_iso(100.f)
        {}
    } _toneMapping;

    bxGdiShaderFx_Instance* _fxI;

};
