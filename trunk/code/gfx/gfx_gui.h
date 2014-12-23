#pragma once

#include "gfx.h"
#include "gui/imgui/imgui.h"

struct bxWindow;

struct bxGfxGUI
{
    bxGfxGUI();

    void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win );

    void update( float deltaTime );
    void draw( bxGfxContext* ctx );

private:
    bxGdiRenderSource* _rsource;
    bxGdiShaderFx_Instance* _fxI;
    bxGdiBuffer _cbuffer;
};
