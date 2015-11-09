#pragma once

#include "gui/imgui/imgui.h"

struct bxWindow;
struct bxGdiDeviceBackend;
struct bxGdiContext;

struct bxGfxGUI
{
    static void _Startup( bxGdiDeviceBackend* dev, bxWindow* win );
    static void _Shutdown( bxGdiDeviceBackend* dev, bxWindow* win );
    
    static void newFrame( float deltaTime );
    static void draw( bxGdiContext* ctx );
};

