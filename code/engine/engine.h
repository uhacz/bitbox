#pragma once

#include "profiler.h"
#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <gfx/gfx_material.h>

struct bxEngine
{
    bxGdiDeviceBackend*   gdiDevice;
    bxGdiContext*         gdiContext;
    bxResourceManager*    resourceManager;

    bxGfxMaterialManager* materialManager;

    //// tool for profiling
    Remotery* _remotery;
};

void bxEngine_startup( bxEngine* e );
void bxEngine_shutdown( bxEngine* e );
