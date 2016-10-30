#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/containers.h>

#include "renderer_type.h"

namespace bx{ namespace gfx{

union MeshMatrix
{
    u8 _single[64] = {};
    Matrix4* _multi;
};

//////////////////////////////////////////////////////////////////////////
struct SceneImpl
{
    void prepare( const char* name, bxAllocator* allocator );
    void unprepare();

    MeshID add( const char* name, u32 numInstances );
    void remove( MeshID* mi );
    MeshID find( const char* name );

    void setRenderSource( MeshID mi, rdi::RenderSource rs );
    void setMaterial( MeshID mi, MaterialID m );
    void setMatrices( MeshID mi, const Matrix4* matrices, u32 count, u32 startIndex = 0 );

private: 
    void _SetToDefaults( u32 index );
    void _AllocateData( u32 newSize, bxAllocator* allocator );
    u32 _GetIndex( MeshID mi );

private:
    struct Data
    {
        void*                _memory_handle = nullptr;
        MeshMatrix*          matrices = nullptr;
        rdi::RenderSource*   render_sources = nullptr;
        MaterialID*          materials = nullptr;
        u32*                 num_instances = nullptr;
        MeshID*              mesh_instance = nullptr;
        char**               names = nullptr;

        u32                  size = 0;
        u32                  capacity = 0;
    }_data;

    const char* _name = nullptr;
    bxAllocator* _allocator = nullptr;

};

}}///
