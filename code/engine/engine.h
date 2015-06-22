#pragma once

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include "profiler.h"

struct bxEngine
{
    bxGdiDeviceBackend*   gdiDevice;
    bxGdiContext*         gdiContext;
    bxResourceManager*    resourceManager;

    //// tool for profiling
    Remotery* _remotery;
};

void bxEngine_startup( bxEngine* e );
void bxEngine_shutdown( bxEngine* e );
