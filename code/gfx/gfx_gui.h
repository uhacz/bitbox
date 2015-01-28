#pragma once

#include "gfx.h"
#include "gui/imgui/imgui.h"

struct bxWindow;
struct bxGfxGUI
{
    static void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );
    static void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );
    
    static void newFrame( float deltaTime );
    static void draw( bxGdiContext* ctx );
};

struct bxGfxShaderFxGUI
{
    u32 flag_isVisible;

    bxGfxShaderFxGUI();

    u32 beginFx( const char* fxName );
    void endFx( u32 id );
    void addInt( bxGdiShaderFx_Instance* fxI, const char* varName, int min = -1, int max = -1 );
    void addFloat( bxGdiShaderFx_Instance* fxI, const char* varName, float min = -1.f, float max = -1.f );
    void addColor( bxGdiShaderFx_Instance* fxI, const char* varName );
};