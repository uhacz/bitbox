#include "renderer_scene.h"
#include <util/debug.h>
#include <util/id_array.h>
#include <util/string_util.h>
#include <util/queue.h>
#include "util/buffer_utils.h"

#include "renderer.h"
#include "util/camera.h"

namespace bx{ namespace gfx{

    static inline MeshID makeMeshID( u32 hash )
    {
        MeshID mi = {};
        mi.i = hash;
        return mi;
    }
    static inline MeshID makeInvalidMeshID()
    {
        return makeMeshID( 0 );
    }


union MeshHandle
{
    enum
    {
        eINDEX_BITS = 24,
        eGENERATION_BITS = 8,
    };

    u32 hash = 0;
    struct
    {
        u32 index : eINDEX_BITS;
        u32 generation : eGENERATION_BITS;
    };
    MeshHandle() {}
    MeshHandle( u32 h ): hash( h ) {}
};
struct MeshHandleManager
{
    enum
    {
        eMINIMUM_FREE_INDICES = 1024,
    };

    queue_t<u32> _free_indices;
    
    array_t<u8>     _generation;
    array_t<Scene>  _scene;
    array_t<u32>    _data_index;

    bxBenaphore _lock;

    MeshID acquire()
    {
        static_assert( sizeof( MeshID ) == sizeof( MeshHandle ), "Handle mismatch" );

        _lock.lock();
        u32 idx;
        if( queue::size( _free_indices ) > eMINIMUM_FREE_INDICES )
        {
            idx = queue::front( _free_indices );
            queue::pop_front( _free_indices );
        }
        else
        {
            idx = array::push_back( _generation, u8( 1 ) );
            array::push_back( _scene, (Scene)nullptr );
            array::push_back( _data_index, UINT32_MAX );
            SYS_ASSERT( idx < ( 1 << MeshHandle::eINDEX_BITS ) );
        }

        _lock.unlock();

        SYS_ASSERT( _scene[idx] == nullptr );
        SYS_ASSERT( _data_index[idx] == UINT32_MAX );

        MeshHandle h;
        h.generation = _generation[idx];
        h.index = idx;
        return makeMeshID( h.hash );
    }
    void release( MeshID* mi )
    {
        MeshHandle* h = (MeshHandle*)mi;
        u32 idx = h->index;
        h->hash = 0;

        ++_generation[idx];
        _scene[idx] = nullptr;
        _data_index[idx] = UINT32_MAX;

        _lock.lock();
        queue::push_back( _free_indices, idx );
        _lock.unlock();
    }

    bool alive( MeshID mi ) const {
        MeshHandle h( mi.i );
        SYS_ASSERT( h.index < array::sizeu( _generation ) );
        return _generation[h.index] == h.generation;
    }

    void setData( MeshID mi, Scene scene, u32 dataIndex )
    {
        MeshHandle h = { mi.i };
        SYS_ASSERT( alive( mi ) );
        _scene[h.index] = scene;
        _data_index[h.index] = dataIndex;
    }

    u32 getDataIndex( MeshID mi ) const
    {
        SYS_ASSERT( alive( mi ) );
        const MeshHandle h( mi.i );
        return _data_index[ h.index ];
    }
    Scene getScene( MeshID mi )
    {
        SYS_ASSERT( alive( mi ) );
        const MeshHandle h( mi.i );
        return _scene[h.index];
    }
};
static MeshHandleManager* g_handle = {};
MeshHandleManager* GMeshHandle() { return g_handle; }

//////////////////////////////////////////////////////////////////////////
void SceneImpl::StartUp()
{
    g_handle = BX_NEW( bxDefaultAllocator(), MeshHandleManager );
}

void SceneImpl::ShutDown()
{
    BX_DELETE0( bxDefaultAllocator(), g_handle );
}
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
    string::free_and_null( (char**)_name );
}

MeshID SceneImpl::Add( const char* name, u32 numInstances )
{
    MeshID mi = Find( name );
    if( GMeshHandle()->alive( mi ) )
    {
        bxLogError( "MeshID with name '%s' already exists", name );
        return makeInvalidMeshID();
    }

    mi = GMeshHandle()->acquire();
    if( _data.size + 1 > _data.capacity )
    {
        _AllocateData( _data.size * 2 + 8, bxDefaultAllocator() );
    }

    const u32 index = _data.size++;
    GMeshHandle()->setData( mi, this, index );
    _SetToDefaults( index );

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

void SceneImpl::Remove( MeshID* mi )
{
    if( !GMeshHandle()->alive( *mi ) )
        return;

    SYS_ASSERT( _data.size > 0 );
    SYS_ASSERT( GMeshHandle()->getScene( *mi ) == this );

    const u32 index = GMeshHandle()->getDataIndex( *mi );
    const u32 last_index = _data.size - 1;

    GMeshHandle()->release( mi );

    if( index != last_index )
    {
        _data.matrices[index]       = _data.matrices[last_index];
        _data.render_sources[index] = _data.render_sources[last_index];
        _data.materials[index]      = _data.materials[last_index];
        _data.num_instances[index]  = _data.num_instances[last_index];
        _data.mesh_instance[index]  = _data.mesh_instance[last_index];
        _data.names[index]          = _data.names[last_index];
        GMeshHandle()->setData( _data.mesh_instance[index], this, index );

        _data.matrices[last_index] = {};
        _data.render_sources[last_index] = BX_RDI_NULL_HANDLE;
        _data.materials[last_index] = {};
        _data.num_instances[last_index] = 0;
        _data.mesh_instance[last_index] = makeInvalidMeshID();
        _data.names[last_index] = nullptr;
    }
}

MeshID SceneImpl::Find( const char* name )
{
    for( u32 i = 0; i < _data.size; ++i )
    {
        if( string::equal( name, _data.names[i] ) )
        {
            return _data.mesh_instance[i];
        }
    }
    return makeInvalidMeshID();
}

void SceneImpl::SetRenderSource( MeshID mi, rdi::RenderSource rs )
{
    const u32 index = _GetIndex( mi );
    _data.render_sources[index] = rs;
}

void SceneImpl::SetMaterial( MeshID mi, MaterialID m )
{
    const u32 index = _GetIndex( mi );
    _data.materials[index] = m;
}

void SceneImpl::SetMatrices( MeshID mi, const Matrix4* matrices, u32 count, u32 startIndex )
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

        rdi::RenderSource rsource = _data.render_sources[i];
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

            rdi::DrawCmd* draw_cmd = rdi::AllocateCommand< rdi::DrawCmd >( cmdb, pipeline_cmd );
            draw_cmd->rsource = rsource;
            draw_cmd->num_instances = num_instances;

            rdi::SubmitCommand( cmdb, instance_cmd, skey.hash );
        }
    }
}

void SceneImpl::_SetToDefaults( u32 index )
{
    string::free_and_null( &_data.names[index] );
    _data.mesh_instance [index] = makeInvalidMeshID();
    _data.render_sources[index] = BX_RDI_NULL_HANDLE;
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
    mem_size += newCapacity * sizeof( *_data.render_sources );
    mem_size += newCapacity * sizeof( *_data.materials );
    mem_size += newCapacity * sizeof( *_data.num_instances );
    mem_size += newCapacity * sizeof( *_data.mesh_instance );
    mem_size += newCapacity * sizeof( *_data.names );

    void* mem = BX_MALLOC( allocator, mem_size, 16 );
    memset( mem, 0x00, mem_size );
    
    Data new_data = {};
    new_data._memory_handle = mem;
    new_data.size = _data.size;
    new_data.capacity = newCapacity;
    
    bxBufferChunker chunker( mem, mem_size );
    new_data.matrices       = chunker.add< MeshMatrix >( newCapacity );
    new_data.render_sources = chunker.add< rdi::RenderSource >( newCapacity );
    new_data.materials      = chunker.add< MaterialID >( newCapacity );
    new_data.num_instances  = chunker.add< u32 >( newCapacity );
    new_data.mesh_instance  = chunker.add< MeshID >( newCapacity );
    new_data.names          = chunker.add< char* >( newCapacity );
    chunker.check();

    if( _data.size )
    {
        BX_CONTAINER_COPY_DATA( &new_data, &_data, matrices );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, render_sources );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, materials );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, num_instances );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, mesh_instance );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, names );
    }

    BX_FREE( allocator, _data._memory_handle );
    _data = new_data;
}

u32 SceneImpl::_GetIndex( MeshID mi )
{
    SYS_ASSERT( GMeshHandle()->alive( mi ) );
    u32 index = GMeshHandle()->getDataIndex( mi );
    SYS_ASSERT( index < _data.size );
    return index;
}

}}///

namespace bx{ namespace gfx{
namespace renderer
{
    void drawScene( Scene scene, const Camera& camera )
    {

    }

    MeshID createMeshID( Scene scene, unsigned numInstances, const char* name /*= nullptr */ )
    {
        return scene->Add( name, numInstances );
    }

    void destroyMeshID( MeshID* mi )
    {
        if( !GMeshHandle()->alive( *mi ) )
            return;

        Scene scene = GMeshHandle()->getScene( *mi );
        scene->Remove( mi );
    }

    void setRenderSource( MeshID mi, rdi::RenderSource rsource )
    {
        Scene scene = GMeshHandle()->getScene( mi );
        scene->SetRenderSource( mi, rsource );
    }
    void setMaterial( MeshID mi, MaterialID  m )
    {
        Scene scene = GMeshHandle()->getScene( mi );
        scene->SetMaterial( mi, m );
    }
}
}}///



namespace bx { namespace gfx {

void VertexTransformDataInit( VertexTransformData* vt, u32 maxInstances )
{
    vt->_offset   = rdi::device::CreateConstantBuffer( sizeof( VertexTransformData::InstanceOffset ) );
    vt->_world    = rdi::device::CreateBufferRO( maxInstances, rdi::Format( rdi::EDataType::FLOAT, 4 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );
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

void VertexTransformDataDeinit( VertexTransformData* vt )
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
