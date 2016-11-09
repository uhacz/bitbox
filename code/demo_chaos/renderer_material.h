#pragma once

#include "renderer_type.h"
#include "renderer_texture.h"

#include <util/debug.h>
#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>

#include <resource_manager/resource_manager.h>

#include <rdi/rdi.h>

namespace bx{ namespace gfx{
    
    //////////////////////////////////////////////////////////////////////////
#define BX_CPP
    typedef float3_t float3;
#include <shaders/shaders/sys/binding_map.h>
#include <shaders/shaders/sys/material.hlsl>
    struct MaterialData
    {
        MATERIAL_DATA_CPP;
    };
    struct MaterialTextures
    {
        MATERIAL_TEXTURES_CPP;
    };
    struct MaterialTextureHandles
    {
        MATERIAL_TEXTURE_HANDLES_CPP;
    };
#undef BX_CPP

    struct Material
    {
        MaterialData data;
        rdi::ConstantBuffer data_cbuffer;
        MaterialTextureHandles htexture;
    };
    void Clear( Material* mat );
    
    //////////////////////////////////////////////////////////////////////////
    namespace material_resource_layout
    {
        static rdi::ResourceBinding bindings[] =
        {
            rdi::ResourceBinding( "MaterialData", rdi::EBindingType::UNIFORM ).Slot( SLOT_MATERIAL_DATA ).StageMask( rdi::EStage::PIXEL_MASK ),
            rdi::ResourceBinding( "diffuse_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE0 ).StageMask( rdi::EStage::PIXEL_MASK ),
            rdi::ResourceBinding( "specular_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE1 ).StageMask( rdi::EStage::PIXEL_MASK ),
            rdi::ResourceBinding( "roughness_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE2 ).StageMask( rdi::EStage::PIXEL_MASK ),
            rdi::ResourceBinding( "metallic_tex", rdi::EBindingType::READ_ONLY ).Slot( SLOT_MATERIAL_TEXTURE3 ).StageMask( rdi::EStage::PIXEL_MASK ),
        };
        static const u32 num_bindings = sizeof( bindings ) / sizeof( *bindings );

        static const rdi::ResourceLayout laytout_notex = { bindings, 1 };
        static const rdi::ResourceLayout laytout_tex = { bindings, num_bindings };
    }///


//////////////////////////////////////////////////////////////////////////
class MaterialContainer
{
public:
    bool Alive( MaterialID m ) const;

    MaterialID Add( const char* name, const Material& mat );
    void Remove( MaterialID* m );

    MaterialID Find( const char* name ) const;

    u32 _GetIndex( MaterialID m ) const;
    const Material& GetMaterial( MaterialID id ) const;

    void SetMaterialData( MaterialID id, const MaterialData& data );

private:
    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    id_t                     _index_to_id[eMAX_COUNT] = {};
    Material                 _material[eMAX_COUNT] = {};
    const char*              _names[eMAX_COUNT] = {};

    inline id_t MakeId( MaterialID m ) const { return make_id( m.i ); }
    inline MaterialID MakeMaterial( id_t id ) const { MaterialID m; m.i = id.hash; return m; }
};

//////////////////////////////////////////////////////////////////////////
MaterialID CreateMaterial( MaterialContainer* container, const char* name, const MaterialData& data, const MaterialTextures& textures );
void DestroyMaterial( MaterialContainer* container, MaterialID* id );
void FillResourceDescriptor( rdi::ResourceDescriptor rdesc, const Material& mat );
//////////////////////////////////////////////////////////////////////////

}}///
