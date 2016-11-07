#pragma once

#include "renderer_type.h"
#include <util/debug.h>
#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>
#include <rdi/rdi.h>

namespace bx{ namespace gfx{

struct MaterialContainer
{
    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    id_t                    _index_to_id[eMAX_COUNT] = {};
    rdi::ResourceDescriptor _rdescs[eMAX_COUNT] = {};
    const char*             _names[eMAX_COUNT] = {};

    inline id_t makeId( MaterialID m ) const { return make_id( m.i ); }
    inline MaterialID makeMaterial( id_t id ) const { MaterialID m; m.i = id.hash; return m; }

    bool alive( MaterialID m ) const;

    MaterialID add( const char* name, rdi::ResourceDescriptor rdesc );
    void remove( MaterialID* m );

    MaterialID find( const char* name );

    u32 _GetIndex( MaterialID m );
    rdi::ResourceDescriptor getResourceDesc( MaterialID m );
};

namespace MATERIALS
{
#define BX_CPP
    typedef float3_t float3;
    typedef rdi::TextureRO texture2D;
#include <shaders/shaders/sys/material.hlsl>
#include <shaders/shaders/sys/binding_map.h>

    static const rdi::ResourceBinding bindings[] =
    {
        rdi::ResourceBinding( "MaterialData", rdi::EBindingType::UNIFORM ).Slot( SLOT_MATERIAL_DATA ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "diffuse_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE0 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "specular_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE1 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "roughness_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE2 ).StageMask( rdi::EStage::PIXEL_MASK ),
        rdi::ResourceBinding( "metallic_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE3 ).StageMask( rdi::EStage::PIXEL_MASK ),
    };

    struct MaterialData
    {
        MATERIAL_DATA_CPP
    };
    struct MaterialTextures
    {
        MATERIAL_TEXTURES_CPP
    };
    struct MaterialTextureObject
    {
        MATERIAL_TEXTURES;
    };

    struct Material
    {
        MaterialData* data = nullptr;
        MaterialTextureObject* textures = nullptr;
    };

    namespace Red
    {
        static Material material = {};
        static const MaterialData data = MaterialData( float3( 1.f, 0.f, 0.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static MaterialTextureObject textures_obj = {};
    }///
    namespace Green
    {
        static const MaterialData data = MaterialData( float3( 0.f, 1.f, 0.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static MaterialTextureObject textures_obj = {};
    }///
    namespace Blue
    {
        static const MaterialData data = MaterialData( float3( 0.f, 0.f, 1.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static MaterialTextureObject textures_obj = {};
    }///

    void CreateMaterials();
    void DestroyMaterials();
    void FillDescriptor( rdi::ResourceDescriptor rdesc, const MaterialData& data );
    void FillDescriptor( rdi::ResourceDescriptor rdesc, const MaterialTextureObject& textures );
}///


}}///