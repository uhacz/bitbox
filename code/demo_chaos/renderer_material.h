#pragma once

#include "renderer_type.h"
#include <util/debug.h>
#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>
#include <rdi/rdi_type.h>
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
    typedef float3_t float3;
    typedef const char* texture2D;
#include <shaders/shaders/sys/material.hlsl>
#include <shaders/shaders/sys/binding_map.h>

    struct MaterialData
    {
        MATERIAL_DATA_CPP
    };
    struct MaterialTextures
    {
        MATERIAL_TEXTURES_CPP
    };
    
    namespace Red
    {
        static const MaterialData data = MaterialData( float3( 1.f, 0.f, 0.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static rdi::ResourceDescriptor desc = BX_RDI_NULL_HANDLE;
    }///
    namespace Green
    {
        static const MaterialData data = MaterialData( float3( 0.f, 1.f, 0.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static rdi::ResourceDescriptor desc = BX_RDI_NULL_HANDLE;
    }///
    namespace Blue
    {
        static const MaterialData data = MaterialData( float3( 0.f, 0.f, 1.f ), 0.5, 0.5, 0.5, 0.5 );
        static const MaterialTextures textures = MaterialTextures( nullptr, nullptr, nullptr, nullptr );
        static rdi::ResourceDescriptor desc = BX_RDI_NULL_HANDLE;
    }///

    void CreateMaterials();
    void DestroyMaterials();
}///


}}///