#pragma once

#include "gfx.h"
#include "gui/imgui/imgui.h"

struct bxWindow;
struct bxGfxGUI
{
    static void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );
    static void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );
    
    static void newFrame( float deltaTime );
    static void draw( bxGdiContext* ctx );
};

