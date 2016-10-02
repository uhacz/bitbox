#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/containers.h>

#include "renderer_type.h"

namespace bx{ namespace gfx{

union MeshInstanceMatrix
{
    u8 _single[64] = {};
    Matrix4* _multi;
};

//////////////////////////////////////////////////////////////////////////
struct SceneImpl
{
    static const u32 cMAX_INSTANCES = 1024;
    id_array_t< cMAX_INSTANCES > _id;

    char*               _names[cMAX_INSTANCES] = {};
    MeshInstance        _mesh_instance[cMAX_INSTANCES] = {};
    MeshInstanceMatrix  _matrices[cMAX_INSTANCES] = {};
    RenderSource        _render_sources[cMAX_INSTANCES] = {};
    Material            _materials[cMAX_INSTANCES] = {};
    u32                 _num_instances[cMAX_INSTANCES] = {};

    void prepare();
    void unprepare();

    MeshInstance add( const char* name, u32 numInstances );
    void remove( MeshInstance mi );
    MeshInstance find( const char* name );

    void setRenderSource( MeshInstance mi, RenderSource rs );
    void setMaterial( MeshInstance mi, Material m );
    void setMatrices( MeshInstance mi, const Matrix4* matrices, u32 count );

private: void _SetToDefaults( u32 index );

};
}}///
