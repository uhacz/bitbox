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

bxGfxMaterialManager* bxGfxMaterialManager::__this = 0;

int bxGfxMaterialManager::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* nativeShaderName )
{
    _nativeFx = bxGdi::shaderFx_createFromFile( dev, resourceManager, nativeShaderName );


    if( _nativeFx )
    {
        bxGfxMaterial_Params params;
        {
            params.diffuseColor = float3_t( 1.f, 0.f, 0.f );
            params.fresnelColor = float3_t( 0.045593921f );
            params.diffuseCoeff = 0.6f;
            params.roughnessCoeff = 0.2f;
            params.specularCoeff = 0.9f;
            params.ambientCoeff = 0.2f;

            createMaterial( dev, resourceManager, "red", params );
        }
        {
            params.diffuseColor = float3_t( 0.f, 1.f, 0.f );
            params.fresnelColor = float3_t( 0.171968833f );
            params.diffuseCoeff = 0.25f;
            params.roughnessCoeff = 0.5f;
            params.specularCoeff = 0.5f;
            createMaterial( dev, resourceManager, "green", params );
        }
        {
            params.diffuseColor = float3_t( 0.f, 0.f, 1.f );
            params.fresnelColor = float3_t( 0.171968833f );
            params.diffuseCoeff = 0.7f;
            params.roughnessCoeff = 0.7f;

            createMaterial( dev, resourceManager, "blue", params );
        }
        {
            params.diffuseColor = float3_t( 1.f, 1.f, 1.f );
            params.diffuseCoeff = 1.0f;
            params.roughnessCoeff = 1.0f;
            params.specularCoeff = 0.1f;

            createMaterial( dev, resourceManager, "white", params );
        }
    }

    SYS_ASSERT( __this == 0 );
    __this = this;

    return _nativeFx ? 0 : -1;
}

void bxGfxMaterialManager::_Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    SYS_ASSERT( __this == this );
    __this = 0;
    hashmap::iterator it( _map );
    while( it.next() )
    {
        bxGdiShaderFx_Instance* fxI = (bxGdiShaderFx_Instance*)it->value;
        bxGdi::shaderFx_releaseInstance( dev, resourceManager, &fxI );
    }
    hashmap::clear( _map );

    bxGdi::shaderFx_release( dev, resourceManager, &_nativeFx );
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

bxGdiShaderFx_Instance* bxGfxMaterialManager::createMaterial( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* name, const bxGfxMaterial_Params& params, const bxGfxMaterial_Textures* textures )
{
    const u64 key = createMaterialNameHash( name );
    SYS_ASSERT( hashmap::lookup( _map, key) == 0 );

    bxGdiShaderFx_Instance* fxI = bxGdi::shaderFx_createInstance( dev, resourceManager, _nativeFx );
    SYS_ASSERT( fxI != 0 );

    setMaterialParams( fxI, params );

    hashmap_t::cell_t* cell = hashmap::insert( _map, key );
    cell->value = (size_t)fxI;

    return fxI;
}

bxGdiShaderFx_Instance* bxGfxMaterialManager::findMaterial(const char* name)
{
    const u64 key = createMaterialNameHash( name );
    hashmap_t::cell_t* cell = hashmap::lookup( __this->_map, key );

    return (cell) ? (bxGdiShaderFx_Instance*)cell->value : 0;
}