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
struct VertexTransformData;
struct SceneImpl
{
    static void StartUp();
    static void ShutDown();

    void Prepare( const char* name, bxAllocator* allocator );
    void Unprepare();

    MeshID Add( const char* name, u32 numInstances );
    void Remove( MeshID* mi );
    MeshID Find( const char* name );

    void SetRenderSource( MeshID mi, rdi::RenderSource rs );
    void SetMaterial( MeshID mi, MaterialID m );
    void SetMatrices( MeshID mi, const Matrix4* matrices, u32 count, u32 startIndex = 0 );

    void BuildCommandBuffer( rdi::CommandBuffer cmdb, VertexTransformData* vtransform );

private: 
    void _SetToDefaults( u32 index );
    void _AllocateData( u32 newSize, bxAllocator* allocator );
    u32  _GetIndex( MeshID mi );

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


#include <rdi/rdi.h>
namespace bx { namespace gfx {

struct VertexTransformData
{
    struct InstanceOffset
    {
        u32 begin;
        u32 padding_[3];
    };

    rdi::ConstantBuffer _offset = {};
    rdi::BufferRO _world = {};
    rdi::BufferRO _world_it = {};
    rdi::ResourceDescriptor _rdesc = BX_RDI_NULL_HANDLE;
    
    u32 _max_instances = 0;
    u32 _num_instances = 0;

    float4_t* _mapped_data_world = nullptr;
    float3_t* _mapped_data_world_it = nullptr;

    void Map( rdi::CommandQueue* cmdq );
    void Unmap( rdi::CommandQueue* cmdq );
    u32  AddBatch( const Matrix4* matrices, u32 count );

    void Bind( rdi::CommandQueue* cmdq );
    void SetCurrent( rdi::CommandQueue* cmdq, u32 index );
    rdi::Command* SetCurrent( rdi::CommandBuffer cmdBuff, u32 index, rdi::Command* parentCmd = nullptr );

};

void VertexTransformDataInit( VertexTransformData* vt, u32 maxInstances );
void VertexTransformDataDeinit( VertexTransformData* vt );

}}///
