#pragma once
#include <system/window.h>

#include <engine/engine.h>

#include <gdi/gdi_shader.h>

#include <gfx/gfx.h>
#include <gfx/gfx_mesh_renderer.h>
#include <gfx/gfx_postprocess.h>
#include <gfx/gfx_material.h>
#include <gfx/gfx_gui.h>

#include "entity.h"

struct bxDemoEngine
{
    bxGfxContext*         gfxContext;
    bxGfxLights*          gfxLights;
    bxGfxPostprocess*     gfxPostprocess;
    bxGfxMaterialManager* gfxMaterials;

    bxGfxLightsGUI        gui_lights;
    bxGfxShaderFxGUI      gui_shaderFx;

    bxEntity_Manager      entityManager;

    static const int MAX_LIGHTS = 64;
    static const int TILE_SIZE = 32;
};


void bxDemoEngine_startup( bxEngine* e, bxDemoEngine* dengine );
void bxDemoEngine_shutdown( bxEngine* e, bxDemoEngine* dengine );
void bxDemoEngine_frameDraw( bxWindow* win, bxEngine* e, bxDemoEngine* dengine, bxGfxRenderList* rList, const bxGfxCamera& camera, float deltaTime );