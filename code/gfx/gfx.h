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
