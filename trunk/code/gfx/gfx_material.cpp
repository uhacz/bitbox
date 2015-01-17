#include "gfx_material.h"
#include <gdi/gdi_shader.h>
#include <util/hashmap.h>
#include <util/hash.h>

bxGfxMaterialManager::bxGfxMaterialManager()
    : _nativeFx(0)
{}

bxGfxMaterialManager::~bxGfxMaterialManager()
{
}

int bxGfxMaterialManager::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    _nativeFx = bxGdi::shaderFx_createFromFile( dev, resourceManager, "native" );


    if( _nativeFx )
    {
        bxGfxMaterial_Params params;
        {
            params.diffuseColor = float3_t( 1.f, 0.f, 0.f );
            params.fresnelColor = float3_t( 0.045593921f );
            params.diffuseCoeff = 0.4f;
            params.roughnessCoeff = 0.5f;
            params.specularCoeff = 0.5f;
            params.ambientCoeff = 0.5f;

            createMaterial( dev, "red", params );
        }
        {
            params.diffuseColor = float3_t( 0.f, 1.f, 0.f );
            params.fresnelColor = float3_t( 0.171968833f );
            params.diffuseCoeff = 0.25f;
            createMaterial( dev, "green", params );
        }
        {
            params.diffuseColor = float3_t( 0.f, 0.f, 1.f );
            params.fresnelColor = float3_t( 0.171968833f );
            params.diffuseCoeff = 0.7f;

            createMaterial( dev, "blue", params );
        }
    }

    return _nativeFx ? 0 : -1;
}

void bxGfxMaterialManager::_Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    hashmap::iterator it( _map );
    while( it.next() )
    {
        bxGdiShaderFx_Instance* fxI = (bxGdiShaderFx_Instance*)it->value;
        bxGdi::shaderFx_releaseInstance( dev, &fxI );
    }
    hashmap::clear( _map );

    bxGdi::shaderFx_release( dev, &_nativeFx );
}

namespace
{
    inline u64 createMaterialNameHash( const char* name )
    {
        const u32 hashedName0 = simple_hash( name );
        const u32 hashedName1 = murmur3_hash32( name, (u32)strlen( name ), hashedName0 );
        const u64 key = u64( hashedName1 ) << 32 | u64( hashedName0 );
        return key;
    }

    void setMaterialParams( bxGdiShaderFx_Instance* fxI, const bxGfxMaterial_Params& params )
    {
        fxI->setUniform( "diffuseColor"  , params.diffuseColor  );
        fxI->setUniform( "fresnelColor"  , params.fresnelColor  );
        fxI->setUniform( "diffuseCoeff"  , params.diffuseCoeff  );
        fxI->setUniform( "roughnessCoeff", params.roughnessCoeff);
        fxI->setUniform( "specularCoeff" , params.specularCoeff );
        fxI->setUniform( "ambientCoeff"  , params.ambientCoeff  );
    }
}

bxGdiShaderFx_Instance* bxGfxMaterialManager::createMaterial( bxGdiDeviceBackend* dev, const char* name, const bxGfxMaterial_Params& params, const bxGfxMaterial_Textures* textures )
{
    const u64 key = createMaterialNameHash( name );
    SYS_ASSERT( hashmap::lookup( _map, key) == 0 );

    bxGdiShaderFx_Instance* fxI = bxGdi::shaderFx_createInstance( dev, _nativeFx );
    SYS_ASSERT( fxI != 0 );

    setMaterialParams( fxI, params );

    hashmap_t::cell_t* cell = hashmap::insert( _map, key );
    cell->value = (size_t)fxI;

    return fxI;
}

bxGdiShaderFx_Instance* bxGfxMaterialManager::findMaterial(const char* name)
{
    const u64 key = createMaterialNameHash( name );
    hashmap_t::cell_t* cell = hashmap::lookup( _map, key );

    return (cell) ? (bxGdiShaderFx_Instance*)cell->value : 0;
}