#pragma once

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>

struct bxEngine
{
    bxGdiDeviceBackend*   gdiDevice;
    bxGdiContext*         gdiContext;
    bxResourceManager*    resourceManager;
};

void bxEngine_startup( bxEngine* e );
void bxEngine_shutdown( bxEngine* e );
