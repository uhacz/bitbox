#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/containers.h>
#include "renderer_type.h"
#include "renderer_camera.h"

namespace bx{ namespace gfx{

union MeshMatrix
{
    u8 _single[64] = {};
    Matrix4* _multi;
};


//////////////////////////////////////////////////////////////////////////
struct VertexTransformData;
struct ActorHandleManager;

struct SceneImpl
{
    void Prepare( const char* name, bxAllocator* allocator );
    void Unprepare();

    ActorID Add( const char* name, u32 numInstances );
    void Remove( ActorID* mi );
    ActorID Find( const char* name );

    //void SetRenderSource( ActorID mi, rdi::RenderSource rs );
    void SetMesh( ActorID actorId, MeshHandle handle );
    void SetMaterial( ActorID mi, MaterialID m );
    void SetMatrices( ActorID mi, const Matrix4* matrices, u32 count, u32 startIndex = 0 );

    void BuildCommandBuffer( rdi::CommandBuffer cmdb, VertexTransformData* vtransform, const Camera& camera );

private:
    //////////////////////////////////////////////////////////////////////////
    void _SetToDefaults( u32 index );
    void _AllocateData( u32 newSize, bxAllocator* allocator );
    u32  _GetIndex( ActorID mi );

    struct Data
    {
        void*                _memory_handle = nullptr;
        MeshMatrix*          matrices = nullptr;
        //rdi::RenderSource*   render_sources = nullptr;
        MeshHandle*          meshes = nullptr;
        MaterialID*          materials = nullptr;
        u32*                 num_instances = nullptr;
        ActorID*              actor_id = nullptr;
        char**               names = nullptr;

        u32                  size = 0;
        u32                  capacity = 0;
    }_data;

    const char* _name = nullptr;
    bxAllocator* _allocator = nullptr;
    ActorHandleManager* _handle_manager = nullptr;

    friend class Renderer;
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


    static void _Init( VertexTransformData* vt, u32 maxInstances );
    static void _Deinit( VertexTransformData* vt );
};

}}///
