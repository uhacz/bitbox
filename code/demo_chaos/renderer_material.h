#pragma once

#include "renderer_type.h"
#include "renderer_texture.h"

#include <util/debug.h>
#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>

//#include <resource_manager/resource_manager.h>

#include <rdi/rdi.h>

namespace bx{ namespace gfx{
    
    //////////////////////////////////////////////////////////////////////////
    typedef float3_t float3;
#include <shaders/shaders/material_data.h>
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
//#undef BX_CPP
    
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

struct MaterialDesc
{
    MaterialData data;
    MaterialTextures textures;
};

struct MaterialPipeline
{
    rdi::Pipeline pipeline = BX_RDI_NULL_HANDLE;
    rdi::ResourceDescriptor resource_desc = BX_RDI_NULL_HANDLE;
};

class MaterialManager
{
public:
    MaterialHandle Create( const char* name, const MaterialDesc& desc );
    void Destroy( MaterialHandle id );
    void DestroyByName( const char* name );
    MaterialHandle Find( const char* name );
    
    MaterialPipeline Pipeline( MaterialHandle id ) const;

    //////////////////////////////////////////////////////////////////////////
    static void _StartUp();
    static void _ShutDown();

private:
    inline id_t MakeId( MaterialHandle m ) const { return make_id( m.i ); }
    inline MaterialHandle MakeMaterial( id_t id ) const { MaterialHandle m; m.i = id.hash; return m; }

    static const u8 MAX_COUNT = 128;
    id_table_t< MAX_COUNT > _id_to_index;
    MaterialPipeline         _material_pipeline[MAX_COUNT] = {};
    MaterialData             _data[MAX_COUNT] = {};
    rdi::ConstantBuffer      _data_cbuffer[MAX_COUNT] = {};
    MaterialTextureHandles   _textures[MAX_COUNT] = {};
    u32                      _hashed_names[MAX_COUNT] = {};


    rdi::Pipeline _pipeline_tex = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_notex = BX_RDI_NULL_HANDLE;

    bxBenaphore _lock;
};

MaterialManager* GMaterialManager();

}}///
