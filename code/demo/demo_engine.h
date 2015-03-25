#pragma once
#include <system/window.h>


#include <resource_manager/resource_manager.h>

#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>

#include <gfx/gfx.h>
#include <gfx/gfx_material.h>
#include <gfx/gfx_gui.h>

#include "entity.h"

struct bxDemoEngine
{
    bxGdiDeviceBackend*   gdiDevice;
    bxGdiContext*         gdiContext;
    bxGfxContext*         gfxContext;
    bxGfxLights*          gfxLights;
    bxGfxPostprocess*     gfxPostprocess;
    bxGfxMaterialManager* gfxMaterials;
    bxResourceManager*    resourceManager;

    bxGfxLightsGUI        gui_lights;
    bxGfxShaderFxGUI      gui_shaderFx;

    bxEntity_Manager      entityManager;

    static const int MAX_LIGHTS = 64;
    static const int TILE_SIZE = 32;
};


void bxDemoEngine_startup( bxDemoEngine* dengine );
void bxDemoEngine_shutdown( bxDemoEngine* dengine );
void bxDemoEngine_frameDraw( bxWindow* win, bxDemoEngine* dengine, bxGfxRenderList* rList, const bxGfxCamera& camera, float deltaTime );