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
    void prepare();
    void unprepare();

    MeshInstance add( const char* name, u32 numInstances );
    void remove( MeshInstance* mi );
    MeshInstance find( const char* name );

    void setRenderSource( MeshInstance mi, RenderSource rs );
    void setMaterial( MeshInstance mi, Material m );
    void setMatrices( MeshInstance mi, const Matrix4* matrices, u32 count, u32 startIndex = 0 );

private: 
    void _SetToDefaults( u32 index );
    void _AllocateData( u32 newSize, bxAllocator* allocator );
    u32 _GetIndex( MeshInstance mi );
private:
    struct Data
    {
        void*                _memory_handle = nullptr;
        MeshInstanceMatrix*  matrices = nullptr;
        RenderSource*        render_sources = nullptr;
        Material*            materials = nullptr;
        u32*                 num_instances = nullptr;
        MeshInstance*        mesh_instance = nullptr;
        char**               names = nullptr;

        u32                  size = 0;
        u32                  capacity = 0;
    }_data;
};

}}///
