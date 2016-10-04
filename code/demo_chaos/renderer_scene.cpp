#include "renderer_scene.h"
#include <util/debug.h>
#include <util/id_array.h>
#include <util/string_util.h>
#include <util/queue.h>
#include "util/buffer_utils.h"

namespace bx{ namespace gfx{

    static inline MeshInstance makeMeshInstance( u32 hash )
    {
        MeshInstance mi = {};
        mi.i = hash;
        return mi;
    }
    static inline MeshInstance makeInvalidMeshInstance()
    {
        return makeMeshInstance( 0 );
    }


union MeshInstanceHandle
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
    MeshInstanceHandle() {}
    MeshInstanceHandle( u32 h ): hash( h ) {}
};
struct MeshInstanceHandleManager
{
    enum
    {
        eMINIMUM_FREE_INDICES = 1024,
    };

    queue_t<u32> _free_indices;
    
    array_t<u8>     _generation;
    array_t<Scene>  _scene;
    array_t<u32>    _data_index;

    MeshInstance acquire()
    {
        static_assert( sizeof( MeshInstance ) == sizeof( MeshInstanceHandle ), "Handle mismatch" );

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
            SYS_ASSERT( idx < ( 1 << MeshInstanceHandle::eINDEX_BITS ) );
        }

        SYS_ASSERT( _scene[idx] == nullptr );
        SYS_ASSERT( _data_index[idx] == UINT32_MAX );

        MeshInstanceHandle h;
        h.generation = _generation[idx];
        h.index = idx;
        return makeMeshInstance( h.hash );
    }
    void release( MeshInstance* mi )
    {
        MeshInstanceHandle* h = (MeshInstanceHandle*)mi;
        u32 idx = h->index;
        h->hash = 0;

        ++_generation[idx];
        _scene[idx] = nullptr;
        _data_index[idx] = UINT32_MAX;
        queue::push_back( _free_indices, idx );
    }

    bool alive( MeshInstance mi ) const {
        MeshInstanceHandle h( mi.i );
        SYS_ASSERT( h.index < array::sizeu( _generation ) );
        return _generation[h.index] == h.generation;
    }

    void setData( MeshInstance mi, Scene scene, u32 dataIndex )
    {
        MeshInstanceHandle h = { mi.i };
        SYS_ASSERT( alive( mi ) );
        _scene[h.index] = scene;
        _data_index[h.index] = dataIndex;
    }

    u32 getDataIndex( MeshInstance mi ) const
    {
        SYS_ASSERT( alive( mi ) );
        const MeshInstanceHandle h( mi.i );
        return _data_index[ h.index ];
    }
    Scene getScene( MeshInstance mi )
    {
        SYS_ASSERT( alive( mi ) );
        const MeshInstanceHandle h( mi.i );
        return _scene[h.index];
    }
};
static MeshInstanceHandleManager g_handle = {};


Matrix4* getMatrixPtr( MeshInstanceMatrix& m, u32 numInstances )
{
    SYS_ASSERT( numInstances > 0 );
    return ( numInstances == 1 ) ? (Matrix4*)m._single : m._multi;
}

void SceneImpl::prepare()
{
}
void SceneImpl::unprepare()
{
}

MeshInstance SceneImpl::add( const char* name, u32 numInstances )
{
    MeshInstance mi = find( name );
    if( g_handle.alive( mi ) )
    {
        bxLogError( "MeshInstance with name '%s' already exists", name );
        return makeInvalidMeshInstance();
    }

    mi = g_handle.acquire();
    if( _data.size + 1 > _data.capacity )
    {
        _AllocateData( _data.size * 2 + 8, bxDefaultAllocator() );
    }

    const u32 index = _data.size++;
    g_handle.setData( mi, this, index );
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

void SceneImpl::remove( MeshInstance* mi )
{
    if( !g_handle.alive( *mi ) )
        return;

    SYS_ASSERT( _data.size > 0 );
    SYS_ASSERT( g_handle.getScene( *mi ) == this );

    const u32 index = g_handle.getDataIndex( *mi );
    const u32 last_index = _data.size - 1;

    g_handle.release( mi );

    if( index != last_index )
    {
        _data.matrices[index]       = _data.matrices[last_index];
        _data.render_sources[index] = _data.render_sources[last_index];
        _data.materials[index]      = _data.materials[last_index];
        _data.num_instances[index]  = _data.num_instances[last_index];
        _data.mesh_instance[index]  = _data.mesh_instance[last_index];
        _data.names[index]          = _data.names[last_index];
        g_handle.setData( _data.mesh_instance[index], this, index );

        _data.matrices[last_index] = {};
        _data.render_sources[last_index] = BX_GFX_NULL_HANDLE;
        _data.materials[last_index] = {};
        _data.num_instances[last_index] = 0;
        _data.mesh_instance[last_index] = makeInvalidMeshInstance();
        _data.names[last_index] = nullptr;
    }
}

MeshInstance SceneImpl::find( const char* name )
{
    for( u32 i = 0; i < _data.size; ++i )
    {
        if( string::equal( name, _data.names[i] ) )
        {
            return _data.mesh_instance[i];
        }
    }
    return makeInvalidMeshInstance();
}

void SceneImpl::setRenderSource( MeshInstance mi, RenderSource rs )
{
    const u32 index = _GetIndex( mi );
    _data.render_sources[index] = rs;
}

void SceneImpl::setMaterial( MeshInstance mi, Material m )
{
    const u32 index = _GetIndex( mi );
    _data.materials[index] = m;
}

void SceneImpl::setMatrices( MeshInstance mi, const Matrix4* matrices, u32 count, u32 startIndex )
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

void SceneImpl::_SetToDefaults( u32 index )
{
    string::free_and_null( &_data.names[index] );
    _data.mesh_instance [index] = makeInvalidMeshInstance();
    _data.render_sources[index] = BX_GFX_NULL_HANDLE;
    _data.materials     [index] = {};
    
    if( _data.num_instances[index] > 1 )
    {
        BX_FREE( bxDefaultAllocator(), _data.matrices[index]._multi );
        memset( &_data.matrices[index], 0x00, sizeof( MeshInstanceMatrix ) );
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
    new_data.matrices       = chunker.add< MeshInstanceMatrix >( newCapacity );
    new_data.render_sources = chunker.add< RenderSource >( newCapacity );
    new_data.materials      = chunker.add< Material >( newCapacity );
    new_data.num_instances  = chunker.add< u32 >( newCapacity );
    new_data.mesh_instance  = chunker.add< MeshInstance >( newCapacity );
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

u32 SceneImpl::_GetIndex( MeshInstance mi )
{
    SYS_ASSERT( g_handle.alive( mi ) );
    u32 index = g_handle.getDataIndex( mi );
    SYS_ASSERT( index < _data.size );
    return index;
}

}
}///