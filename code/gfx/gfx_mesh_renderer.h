#pragma once

#include "gfx_render_list.h"
#include "gfx_sort_list.h"
#include "gfx_shadows.h"
#include "gfx_lights.h"

class bxResourceManager;
struct bxGfxContext
{
    bxGfxContext();
    ~bxGfxContext();

    int _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void bindCamera( bxGdiContext* ctx, const bxGfxCamera& camera, int rtWidth = -1, int rtHeight = -1 );

    void frame_begin( bxGdiContext* ctx );
    void frame_zPrepass( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists );
    void frame_drawShadows( bxGdiContext* ctx, bxGfxRenderList** rLists, int numLists, const bxGfxCamera& camera, const bxGfxLights& lights );
    void frame_drawColor( bxGdiContext* ctx, const bxGfxCamera& camera, bxGfxRenderList** rLists, int numLists, bxGdiTexture ssaoTexture );
    void frame_end( bxGdiContext* ctx );

    bxGdiTexture framebuffer( int index ) { SYS_ASSERT( index < bxGfx::eFRAMEBUFFER_COUNT );  return _framebuffer[index]; }
    int framebufferWidth() const { return _framebuffer->width; }
    int framebufferHeight() const { return _framebuffer->height; }
    
private:
    bxGdiBuffer _cbuffer_frameData;
    bxGdiBuffer _cbuffer_instanceData;

    bxGdiTexture _framebuffer[bxGfx::eFRAMEBUFFER_COUNT];

    bxGfxSortList_Color* _sortList_color;
    bxGfxSortList_Depth* _sortList_depth;
    bxGfxSortList_Shadow* _sortList_shadow;

    bxGfxShadows _shadows;
    f32 _scene_zRange[2];

    //static bxGfx::Shared _shared;
};