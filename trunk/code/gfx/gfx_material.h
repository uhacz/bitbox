#pragma once

#include <util/type.h>
#include <util/containers.h>
#include <gdi/gdi_backend.h>

struct bxGdiShaderFx;
struct bxGdiShaderFx_Instance;
class bxResourceManager;

namespace bxGfx
{
    namespace internal
    {
        typedef float3_t float3;
        #include <shaders/hlsl/sys/material.hlsl>
    }///
}///
typedef bxGfx::internal::Material bxGfxMaterial_Params;

struct bxGfxMaterial_Textures
{
    bxGdiTexture albedo;
};

////
////
struct bxGfxMaterialManager
{
    bxGdiShaderFx_Instance* createMaterial( bxGdiDeviceBackend* dev, const char* name, const bxGfxMaterial_Params& params, const bxGfxMaterial_Textures* textures = 0 );
    bxGdiShaderFx_Instance* findMaterial( const char* name );

public:
    bxGfxMaterialManager();
    ~bxGfxMaterialManager();
    int _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

private:
    typedef hashmap_t Map_Material;
    Map_Material _map;

    bxGdiShaderFx* _nativeFx;
};

