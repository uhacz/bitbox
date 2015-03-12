#include "entity.h"
#include <util/array.h>
#include <util/queue.h>
#include <util/debug.h>
#include <util/random.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>
#include <gdi/gdi_render_source.h>
#include <string.h>

bxEntity bxEntityManager::create()
{
    u32 idx;
    if( queue::size( _freeIndeices ) > eMINIMUM_FREE_INDICES )
    {
        idx = queue::front( _freeIndeices );
        queue::pop_front( _freeIndeices );
    }
    else
    {
        idx = array::push_back( _generation, u8(0) );
        SYS_ASSERT( idx < (1 << bxEntity::eINDEX_BITS) );
    }

    bxEntity e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntityManager::release( bxEntity* e )
{
    u32 idx = e->index;
    e->hash = 0;

    ++_generation[idx];
    queue::push_back( _freeIndeices, idx );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


bxMeshComponentManager::bxMeshComponentManager()
    : _alloc_singleMatrix(0)
    , _alloc_multipleMatrix(0)
    , _alloc_main(0)
    , _memoryHandle(0)
{
    memset( &_data, 0x00, sizeof( InstanceData ) );
}

void bxMeshComponentManager::_SetDefaults()
{
    SYS_ASSERT( _alloc_main == NULL );
    
    _alloc_main = bxDefaultAllocator();
    _alloc_multipleMatrix = bxDefaultAllocator();
    
    bxDynamicPoolAllocator* alloc = BX_NEW( _alloc_main, bxDynamicPoolAllocator );
    alloc->startup( sizeof(Matrix4), 64, _alloc_main, ALIGNOF( Matrix4 ) );
    _alloc_singleMatrix = alloc;
}

void bxMeshComponentManager::_Allocate( int newCapacity )
{
    if( !_alloc_main )
    {
        SYS_ASSERT( _memoryHandle == 0 );
        _SetDefaults();
    }

    if( newCapacity <= _data.capacity )
        return;

    int memSize = 0;
    memSize += newCapacity * sizeof( *_data.rsource );
    memSize += newCapacity * sizeof( *_data.fxI );
    memSize += newCapacity * sizeof( *_data.surface );
    memSize += newCapacity * sizeof( *_data.localAABB );
    memSize += newCapacity * sizeof( *_data.matrix );

    void* mem = BX_MALLOC( _alloc_main, memSize, 8 );
    memset( mem, 0x00, memSize );

    InstanceData newData;
    memset( &newData, 0x00, sizeof(InstanceData) );
    bxBufferChunker chunker( mem, memSize );
    newData.rsource   = chunker.add< bxGdiRenderSource* >( newCapacity );
    newData.fxI       = chunker.add< bxGdiShaderFx_Instance* >( newCapacity );
    newData.surface   = chunker.add< bxGdiRenderSurface >( newCapacity );
    newData.localAABB = chunker.add< bxAABB >( newCapacity );
    newData.matrix    = chunker.add< bxMeshComponent_Matrix >( newCapacity );
    newData.n         = _data.n;
    newData.capacity  = newCapacity;

    if( _memoryHandle )
    {
        memcpy( newData.rsource   , _data.rsource   , _data.n * sizeof(*_data.rsource    ) );
        memcpy( newData.fxI       , _data.fxI       , _data.n * sizeof(*_data.fxI        ) );
        memcpy( newData.surface   , _data.surface   , _data.n * sizeof(*_data.surface    ) );
        memcpy( newData.localAABB , _data.localAABB , _data.n * sizeof(*_data.localAABB  ) );
        memcpy( newData.matrix    , _data.matrix    , _data.n * sizeof(*_data.matrix     ) );

        BX_FREE0( _alloc_main, _memoryHandle );
    }

    _data = newData;
    _memoryHandle = mem;
}
