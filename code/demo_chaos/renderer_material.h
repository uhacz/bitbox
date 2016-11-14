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
//#undef BX_CPP

    struct Material
    {
        MaterialData data;
        MaterialTextureHandles htexture;

        rdi::ConstantBuffer data_cbuffer;
        rdi::ResourceDescriptor resource_desc;
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
class MaterialManager
{
public:
    bool Alive( MaterialID m ) const;
    MaterialID Find( const char* name ) const;
    const Material& Get( MaterialID id ) const;
    rdi::ResourceDescriptor GetResourceDescriptor( MaterialID id ) const;
    void SetMaterialData( MaterialID id, const MaterialData& data );

    MaterialID Create( const char* name, const MaterialData& data, const MaterialTextures& textures );
    void Destroy( MaterialID* id );

private:
    MaterialID Add( const char* name );
    void Remove( MaterialID* m );

    u32 _GetIndex( MaterialID m ) const;
    void _Set( MaterialID id, const Material& m );

    inline id_t MakeId( MaterialID m ) const { return make_id( m.i ); }
    inline MaterialID MakeMaterial( id_t id ) const { MaterialID m; m.i = id.hash; return m; }

private:
    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    id_t                     _index_to_id[eMAX_COUNT] = {};
    Material                 _material[eMAX_COUNT] = {};
    const char*              _names[eMAX_COUNT] = {};
};

//////////////////////////////////////////////////////////////////////////
void MaterialManagerStartUp();
void MaterialManagerShutDown();
MaterialManager* GMaterialManager();


//////////////////////////////////////////////////////////////////////////
void FillResourceDescriptor( rdi::ResourceDescriptor rdesc, const Material& mat );
//////////////////////////////////////////////////////////////////////////

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

class MaterialManager1
{
public:
    MaterialID Create( const char* name, const MaterialDesc& desc );
    void Destroy( MaterialID id );
    MaterialID Find( const char* name );
    
    MaterialPipeline Pipeline( MaterialID id ) const;

    //////////////////////////////////////////////////////////////////////////
    static void _StartUp();
    static void _ShutDown();

private:
    inline id_t MakeId( MaterialID m ) const { return make_id( m.i ); }
    inline MaterialID MakeMaterial( id_t id ) const { MaterialID m; m.i = id.hash; return m; }

    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    MaterialPipeline         _material_pipeline[eMAX_COUNT] = {};
    MaterialData             _data[eMAX_COUNT] = {};
    MaterialTextureHandles   _textures[eMAX_COUNT] = {};

    rdi::Pipeline _pipeline_tex = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_notex = BX_RDI_NULL_HANDLE;

    bxBenaphore _lock;
};

MaterialManager1* GMaterialManager1();

}}///
