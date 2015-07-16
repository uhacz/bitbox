#pragma once

#include <gdi/gdi_backend.h>

////
////
class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiContext;
struct bxGfxCamera;

struct bxGfx_GlobalResources
{
    struct{
        bxGdiShaderFx_Instance* utils;
        bxGdiShaderFx_Instance* texUtils;
    } fx;

    struct{
        bxGdiRenderSource* fullScreenQuad;
        bxGdiRenderSource* sphere;
        bxGdiRenderSource* box;
    } mesh;
};
namespace bxGfx
{
    void globalResources_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void globalResources_shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    bxGfx_GlobalResources* globalResources();

    void submitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName );
    void copyTexture_RGBA( bxGdiContext* ctx, bxGdiTexture outputTexture, bxGdiTexture inputTexture );
    void rasterizeFramebuffer( bxGdiContext* ctx, bxGdiTexture colorFB, const bxGfxCamera& camera );
}///

