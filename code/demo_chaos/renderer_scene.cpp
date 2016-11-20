#include "renderer_scene.h"
#include <util/debug.h>
#include <util/id_array.h>
#include <util/string_util.h>
#include <util/queue.h>
#include "util/buffer_utils.h"

#include "renderer.h"
#include "renderer_scene_actor.h"
#include "util/camera.h"
#include "util/ring_buffer.h"

namespace bx{ namespace gfx{

//////////////////////////////////////////////////////////////////////////
Matrix4* getMatrixPtr( MeshMatrix& m, u32 numInstances )
{
    SYS_ASSERT( numInstances > 0 );
    return ( numInstances == 1 ) ? (Matrix4*)m._single : m._multi;
}


void SceneImpl::Prepare( const char* name, bxAllocator* allocator )
{
    _name = string::duplicate( nullptr, name );
    _allocator = ( allocator ) ? allocator : bxDefaultAllocator();
}
void SceneImpl::Unprepare()
{
    _allocator = nullptr;
    string::free_and_null( (char**)&_name );
}

ActorID SceneImpl::Add( const char* name, u32 numInstances )
{
    ActorID mi = Find( name );
    if( _handle_manager->alive( mi ) )
    {
        bxLogError( "MeshID with name '%s' already exists", name );
        return makeInvalidMeshID();
    }

    mi = _handle_manager->acquire();
    if( _data.size + 1 > _data.capacity )
    {
        _AllocateData( _data.size * 2 + 8, bxDefaultAllocator() );
    }

    const u32 index = _data.size++;
    _handle_manager->setData( mi, this, index );
    _SetToDefaults( index );

    _data.actor_id[index] = mi;
    _data.names[index] = string::duplicate( nullptr, name );
    _data.num_instances[index] = numInstances;
    if( numInstances > 1 )
    {
        u32 mem_size = numInstances * sizeof( Matrix4 );
        _data.matrices[index]._multi = (Matrix4*)BX_MALLOC( bxDefaultAllocator(), mem_size, 16 );
    }

    Matrix4* matrices = getMatrixPtr( _data.matrices[index], numInstances );
    for( u32 i = 0; i < numInstances; ++i )
    {
        matrices[i] = Matrix4::identity();
    }

    return mi;
}

void SceneImpl::Remove( ActorID* mi )
{
    if( !_handle_manager->alive( *mi ) )
        return;

    SYS_ASSERT( _data.size > 0 );
    SYS_ASSERT( _handle_manager->getScene( *mi ) == this );

    const u32 index = _handle_manager->getDataIndex( *mi );
    const u32 last_index = --_data.size;

    _handle_manager->release( mi );

    if( index != last_index )
    {
        _data.matrices[index]       = _data.matrices[last_index];
        //_data.render_sources[index] = _data.render_sources[last_index];
        _data.meshes[index]         = _data.meshes[last_index];
        _data.materials[index]      = _data.materials[last_index];
        _data.num_instances[index]  = _data.num_instances[last_index];
        _data.actor_id[index]  = _data.actor_id[last_index];
        _data.names[index]          = _data.names[last_index];
        _handle_manager->setData( _data.actor_id[index], this, index );

        _data.matrices[last_index] = {};
        _data.meshes[last_index].i = 0;
        _data.materials[last_index] = {};
        _data.num_instances[last_index] = 0;
        _data.actor_id[last_index] = makeInvalidMeshID();
        _data.names[last_index] = nullptr;
    }
}

ActorID SceneImpl::Find( const char* name )
{
    for( u32 i = 0; i < _data.size; ++i )
    {
        if( string::equal( name, _data.names[i] ) )
        {
            return _data.actor_id[i];
        }
    }
    return makeInvalidMeshID();
}

void SceneImpl::SetMesh( ActorID actorId, MeshHandle handle )
{
    const u32 index = _GetIndex( actorId );
    _data.meshes[index] = handle;
}

//void SceneImpl::SetRenderSource( ActorID mi, rdi::RenderSource rs )
//{
//    const u32 index = _GetIndex( mi );
//    _data.render_sources[index] = rs;
//}

void SceneImpl::SetMaterial( ActorID mi, MaterialID m )
{
    const u32 index = _GetIndex( mi );
    _data.materials[index] = m;
}

void SceneImpl::SetMatrices( ActorID mi, const Matrix4* matrices, u32 count, u32 startIndex )
{
    const u32 index = _GetIndex( mi );
    const u32 num_instances = _data.num_instances[index];
    Matrix4* data = getMatrixPtr( _data.matrices[index], num_instances );
    SYS_ASSERT( startIndex + count <= num_instances );
    for( u32 i = 0; i < count; ++i )
    {
        data[startIndex + i] = matrices[i];
    }
}

namespace renderer_scene_internal
{
    union SortKey
    {
        u64 hash = 0;
        struct
        {
            u64 depth : 32;
            u64 material : 32;
        };
    };
}///

void SceneImpl::BuildCommandBuffer( rdi::CommandBuffer cmdb, VertexTransformData* vtransform, const Camera& camera )
{
    for( u32 i = 0; i < _data.size; ++i )
    {
        const u32 num_instances = _data.num_instances[i];
        Matrix4* matrices = getMatrixPtr( _data.matrices[i], num_instances );

        MeshHandle hmesh = _data.meshes[i];
        rdi::RenderSource rsource = GMeshManager()->RenderSource( hmesh );
        MaterialPipeline material_pipeline = GMaterialManager()->Pipeline( _data.materials[i] );

        for( u32 imatrix = 0; imatrix < num_instances; ++imatrix )
        {
            const Matrix4& matrix = matrices[imatrix];
            const float depth = cameraDepth( camera.world, matrix.getTranslation() ).getAsFloat();

            const u32 batch_offset = vtransform->AddBatch( &matrix, 1 );

            renderer_scene_internal::SortKey skey;
            skey.depth = TypeReinterpert( depth ).u;
            skey.material = _data.materials[i].i;

            rdi::Command* instance_cmd = vtransform->SetCurrent( cmdb, batch_offset, nullptr );
            
            rdi::SetPipelineCmd* pipeline_cmd = rdi::AllocateCommand<rdi::SetPipelineCmd>( cmdb, instance_cmd );
            pipeline_cmd->pipeline = material_pipeline.pipeline;

            rdi::SetResourcesCmd* resources_cmd = rdi::AllocateCommand<rdi::SetResourcesCmd>( cmdb, pipeline_cmd );
            resources_cmd->desc = material_pipeline.resource_desc;

            rdi::DrawCmd* draw_cmd = rdi::AllocateCommand< rdi::DrawCmd >( cmdb, resources_cmd );
            draw_cmd->rsource = rsource;
            draw_cmd->num_instances = 1;

            rdi::SubmitCommand( cmdb, instance_cmd, skey.hash );
        }
    }
}

void SceneImpl::_SetToDefaults( u32 index )
{
    string::free_and_null( &_data.names[index] );
    _data.actor_id [index] = makeInvalidMeshID();
    _data.meshes[index].i = 0;
    _data.materials     [index] = {};
    
    if( _data.num_instances[index] > 1 )
    {
        BX_FREE( bxDefaultAllocator(), _data.matrices[index]._multi );
        memset( &_data.matrices[index], 0x00, sizeof( MeshMatrix ) );
    }
    _data.num_instances[index] = 0;
}

void SceneImpl::_AllocateData( u32 newCapacity, bxAllocator* allocator )
{
    u32 mem_size = 0;
    mem_size += newCapacity * sizeof( *_data.matrices );
    mem_size += newCapacity * sizeof( *_data.meshes );
    mem_size += newCapacity * sizeof( *_data.materials );
    mem_size += newCapacity * sizeof( *_data.num_instances );
    mem_size += newCapacity * sizeof( *_data.actor_id );
    mem_size += newCapacity * sizeof( *_data.names );

    void* mem = BX_MALLOC( allocator, mem_size, 16 );
    memset( mem, 0x00, mem_size );
    
    Data new_data = {};
    new_data._memory_handle = mem;
    new_data.size = _data.size;
    new_data.capacity = newCapacity;
    
    bxBufferChunker chunker( mem, mem_size );
    new_data.matrices       = chunker.add< MeshMatrix >( newCapacity );
    new_data.meshes         = chunker.add< MeshHandle >( newCapacity );
    new_data.materials      = chunker.add< MaterialID >( newCapacity );
    new_data.num_instances  = chunker.add< u32 >( newCapacity );
    new_data.actor_id  = chunker.add< ActorID >( newCapacity );
    new_data.names          = chunker.add< char* >( newCapacity );
    chunker.check();

    if( _data.size )
    {
        BX_CONTAINER_COPY_DATA( &new_data, &_data, matrices );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, meshes );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, materials );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, num_instances );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, actor_id );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, names );
    }

    BX_FREE( allocator, _data._memory_handle );
    _data = new_data;
}

u32 SceneImpl::_GetIndex( ActorID mi )
{
    SYS_ASSERT( _handle_manager->alive( mi ) );
    u32 index = _handle_manager->getDataIndex( mi );
    SYS_ASSERT( index < _data.size );
    return index;
}

}}///

namespace bx { namespace gfx {

void VertexTransformData::_Init( VertexTransformData* vt, u32 maxInstances )
{
    vt->_offset = rdi::device::CreateConstantBuffer( sizeof( VertexTransformData::InstanceOffset ) );
    vt->_world = rdi::device::CreateBufferRO( maxInstances, rdi::Format( rdi::EDataType::FLOAT, 4 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );
    vt->_world_it = rdi::device::CreateBufferRO( maxInstances, rdi::Format( rdi::EDataType::FLOAT, 3 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );

    rdi::ResourceBinding bindings[] =
    {
        rdi::ResourceBinding( "offset", rdi::EBindingType::UNIFORM ).StageMask( rdi::EStage::VERTEX_MASK ).Slot( SLOT_INSTANCE_OFFSET ),
        rdi::ResourceBinding( "world", rdi::EBindingType::READ_ONLY ).StageMask( rdi::EStage::VERTEX_MASK ).Slot( SLOT_INSTANCE_DATA_WORLD ),
        rdi::ResourceBinding( "world_it", rdi::EBindingType::READ_ONLY ).StageMask( rdi::EStage::VERTEX_MASK ).Slot( SLOT_INSTANCE_DATA_WORLD_IT ),
    };

    rdi::ResourceLayout layout = {};
    layout.bindings = bindings;
    layout.num_bindings = 3;
    vt->_rdesc = rdi::CreateResourceDescriptor( layout );

    rdi::SetConstantBuffer( vt->_rdesc, "offset", &vt->_offset );
    rdi::SetResourceRO( vt->_rdesc, "world", &vt->_world );
    rdi::SetResourceRO( vt->_rdesc, "world_it", &vt->_world_it );

    vt->_max_instances = maxInstances;
}

void VertexTransformData::_Deinit( VertexTransformData* vt )
{
    rdi::DestroyResourceDescriptor( &vt->_rdesc );
    rdi::device::DestroyBufferRO( &vt->_world_it );
    rdi::device::DestroyBufferRO( &vt->_world );
    rdi::device::DestroyConstantBuffer( &vt->_offset );
}

void VertexTransformData::Map( rdi::CommandQueue* cmdq )
{
    SYS_ASSERT( _mapped_data_world == nullptr );
    SYS_ASSERT( _mapped_data_world_it == nullptr );
    _mapped_data_world = (float4_t*)rdi::context::Map( cmdq, _world, 0, rdi::EMapType::WRITE );
    _mapped_data_world_it = (float3_t*)rdi::context::Map( cmdq, _world_it, 0, rdi::EMapType::WRITE );

    _num_instances = 0;
}
void VertexTransformData::Unmap( rdi::CommandQueue* cmdq )
{
    SYS_ASSERT( _mapped_data_world != nullptr );
    SYS_ASSERT( _mapped_data_world_it != nullptr );

    rdi::context::Unmap( cmdq, _world_it );
    rdi::context::Unmap( cmdq, _world );

    _mapped_data_world = nullptr;
    _mapped_data_world_it = nullptr;
}
u32 VertexTransformData::AddBatch( const Matrix4* matrices, u32 count )
{
    if( _num_instances + count > _max_instances )
        return UINT32_MAX;

    u32 offset = _num_instances;
    _num_instances += count;

    for( u32 imatrix = 0; imatrix < count; ++imatrix )
    {
        const u32 dataOffset = ( offset + imatrix ) * 3;
        const Matrix4 worldRows = transpose( matrices[imatrix] );
        const Matrix3 worldITRows = inverse( matrices[imatrix].getUpper3x3() );

        const float4_t* worldRowsPtr = (float4_t*)&worldRows;
        memcpy( _mapped_data_world + dataOffset, worldRowsPtr, sizeof( float4_t ) * 3 );

        const float4_t* worldITRowsPtr = (float4_t*)&worldITRows;
        memcpy( _mapped_data_world_it + dataOffset + 0, worldITRowsPtr + 0, sizeof( float3_t ) );
        memcpy( _mapped_data_world_it + dataOffset + 1, worldITRowsPtr + 1, sizeof( float3_t ) );
        memcpy( _mapped_data_world_it + dataOffset + 2, worldITRowsPtr + 2, sizeof( float3_t ) );
    }
    return offset;
}
void VertexTransformData::Bind( rdi::CommandQueue* cmdq )
{
    rdi::BindResources( cmdq, _rdesc );
}

void VertexTransformData::SetCurrent( rdi::CommandQueue* cmdq, u32 index )
{
    InstanceOffset off = {};
    off.begin = index;
    rdi::context::UpdateCBuffer( cmdq, _offset, &off );
}

rdi::Command* VertexTransformData::SetCurrent( rdi::CommandBuffer cmdBuff, u32 index, rdi::Command* parentCmd )
{
    rdi::UpdateConstantBufferCmd* cmd = rdi::AllocateCommand<rdi::UpdateConstantBufferCmd>( cmdBuff, sizeof( InstanceOffset ), parentCmd );
    cmd->cbuffer = _offset;
    InstanceOffset* off = (InstanceOffset*)cmd->DataPtr();
    memset( off, 0x00, sizeof( InstanceOffset ) );
    off->begin = index;

    return cmd;
}


}}///
