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
    static bxGdiShaderFx_Instance* findMaterial( const char* name );

public:
    bxGfxMaterialManager();
    ~bxGfxMaterialManager();
    int _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* nativeShaderName = "native" );
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    
    bxGdiShaderFx_Instance* createMaterial( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* name, const bxGfxMaterial_Params& params, const bxGfxMaterial_Textures* textures = 0 );
private:
    typedef hashmap_t Map_Material;
    Map_Material _map;

    bxGdiShaderFx* _nativeFx;

    static bxGfxMaterialManager* __this;
};

