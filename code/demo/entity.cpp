#include "entity.h"
#include <util/array.h>
#include <util/array_util.h>
#include <util/queue.h>
#include <util/hashmap.h>
#include <util/debug.h>
#include <util/random.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>
#include <gdi/gdi_render_source.h>
#include <gfx/gfx_type.h>
#include <string.h>

bxEntity bxEntity_Manager::create()
{
    u32 idx;
    if( queue::size( _freeIndeices ) > eMINIMUM_FREE_INDICES )
    {
        idx = queue::front( _freeIndeices );
        queue::pop_front( _freeIndeices );
    }
    else
    {
        idx = array::push_back( _generation, u8(1) );
        SYS_ASSERT( idx < (1 << bxEntity::eINDEX_BITS) );
    }

    bxEntity e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntity_Manager::release( bxEntity* e )
{
    for( int icb = 0; icb < array::size( _callback_releaseEntity ); ++icb )
    {
        Callback cb = _callback_releaseEntity[icb];
        bxEntity_releaseCallback* ptr = (bxEntity_releaseCallback*)( cb.ptr );
        (*ptr)( *e, cb.userData );
    }
    
    u32 idx = e->index;
    e->hash = 0;

    ++_generation[idx];
    queue::push_back( _freeIndeices, idx );
}

bxEntity_Manager::bxEntity_Manager()
{

}

bxEntity_Manager::~bxEntity_Manager()
{

}

void bxEntity_Manager::register_releaseCallback( bxEntity_releaseCallback* cb, void* userData )
{
    struct Cmp{
        
        Callback _cb;
        Cmp( Callback cb ): _cb(cb) {} 

        bool operator() ( const Callback& a ){
            return a.ptr == _cb.ptr && a.userData == _cb.userData;
        }
    };    

    Callback entry;
    entry.ptr = cb;
    entry.userData = userData;

    const int foundIndex = array::find1( array::begin( _callback_releaseEntity ), array::end( _callback_releaseEntity ), Cmp(entry) );
    if( foundIndex != -1 )
    {
        bxLogWarning( "Callback (0x%p) already registered", cb );
        return;
    }
    array::push_back( _callback_releaseEntity, entry );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bxComponent_Matrix bxComponent_MatrixAllocator::alloc( int nInstances )
{
    bxAllocator* mxAlloc = (nInstances == 1) ? _alloc_singleMatrix : _alloc_multipleMatrix;
    
    bxComponent_Matrix mx;
    mx.pose = (Matrix4*)mxAlloc->alloc( sizeof( Matrix4 ), ALIGNOF( Matrix4 ) );
    mx.n = nInstances;

    return mx;
}

void bxComponent_MatrixAllocator::free( bxComponent_Matrix* mx )
{
    bxAllocator* mxAlloc = ( mx->n == 1 ) ? _alloc_singleMatrix : _alloc_multipleMatrix;
    mxAlloc->free( mx->pose );

    mx->pose = 0;
    mx->n = 0;
}

bxComponent_MatrixAllocator::bxComponent_MatrixAllocator()
    : _alloc_main(0)
    , _alloc_singleMatrix(0)
    , _alloc_multipleMatrix(0)
{}

void bxComponent_MatrixAllocator::_Startup( bxAllocator* allocMain )
{
    SYS_ASSERT( _alloc_main == NULL );

    _alloc_main = allocMain;
    _alloc_multipleMatrix = allocMain;

    bxDynamicPoolAllocator* alloc = BX_NEW( _alloc_main, bxDynamicPoolAllocator );
    alloc->startup( sizeof( Matrix4 ), 64, _alloc_main, ALIGNOF( Matrix4 ) );
    _alloc_singleMatrix = alloc;
}

void bxComponent_MatrixAllocator::_Shutdown()
{
    bxDynamicPoolAllocator* alloc = (bxDynamicPoolAllocator*)_alloc_singleMatrix;
    alloc->shutdown();

    _alloc_singleMatrix = 0;
    _alloc_multipleMatrix = 0;
    _alloc_main = 0;
}

namespace
{
    inline bxMeshComponent_Instance makeInstance( unsigned i )
    {
        bxMeshComponent_Instance cmpI;
        cmpI.i = (u32)i;
        return cmpI;
    }

    inline size_t entity_toKey( bxEntity e ) { return size_t( e.hash );  }
}
bxMeshComponent_Instance bxMeshComponent_Manager::create( bxEntity e, int nInstances )
{
    if( _data.size + 1 >= _data.capacity )
    {
        const int n = (_data.size) ? _data.size * 2 : 16;
        _Allocate( n );
    }
    
    const int index = _data.size++;
    SYS_ASSERT( _data.entity[index] == bxEntity_null() );
    SYS_ASSERT( hashmap::lookup( _entityMap, entity_toKey( e ) ) == NULL );
    
    InstanceIndices::Handle handle = _indices.add( index );
    hashmap_t::cell_t* cell = hashmap::insert( _entityMap, entity_toKey( e ) );
    cell->value = size_t(handle.asU32());

    SYS_ASSERT( nInstances > 0 );
    bxComponent_Matrix mx = _alloc_matrix.alloc( nInstances );
    _data.matrix[index] = mx;

    bxMeshComponent_Data meshData;
    meshData.rsource = 0;
    meshData.shader = 0;
    meshData.mask = bxGfx::eRENDER_MASK_ALL;
    meshData.layer = bxGfx::eRENDER_LAYER_MIDDLE;
    meshData.passIndex = 0;
    meshData.surface = bxGdiRenderSurface();

    _data.entity[index] = e;
    _data.mesh[index] = meshData;
    _data.localAABB[index] = bxAABB( Vector3( -.5f ), Vector3( .5f ) );
    
    return makeInstance( handle.asU32() );
}
void bxMeshComponent_Manager::release( bxEntity e )
{
    bxMeshComponent_Instance i = lookup( e );
    if( i.i == 0 )
        return;

    release( i );
}
void bxMeshComponent_Manager::release( bxMeshComponent_Instance i )
{
    InstanceIndices::Handle handle( i.i );
    if ( !_indices.isValid( handle ) )
        return;

    const int lastIndex = _data.size - 1;
    const int index = packedArrayHandle_remove( &_indices, handle, lastIndex );
    
    bxEntity entity = _data.entity[index];
    SYS_ASSERT( hashmap::lookup( _entityMap, entity_toKey( entity ) ) != NULL );
    hashmap::eraseByKey( _entityMap, entity_toKey( entity ) );

    _alloc_matrix.free( _data.matrix + index );

    _data.entity[index]    = _data.entity[lastIndex];
    _data.mesh[index]      = _data.mesh[lastIndex];;
    _data.matrix[index]    = _data.matrix[lastIndex];;
    _data.localAABB[index] = _data.localAABB[lastIndex];;

    --_data.size;
}



bxMeshComponent_Instance bxMeshComponent_Manager::lookup(bxEntity e)
{
    hashmap_t::cell_t* cell = hashmap::lookup( _entityMap, entity_toKey( e ) );
    bxMeshComponent_Instance nullInstance = { 0 };
    return (cell) ? makeInstance( u32( cell->value ) ) : nullInstance;
}

bxMeshComponent_Data bxMeshComponent_Manager::mesh( bxMeshComponent_Instance i )
{
    const int index = _GetIndex( i );
    return _data.mesh[index];
}

void bxMeshComponent_Manager::setMesh( bxMeshComponent_Instance i, const bxMeshComponent_Data data )
{
    const int index = _GetIndex( i );
    _data.mesh[index] = data;
}

bxComponent_Matrix bxMeshComponent_Manager::matrix( bxMeshComponent_Instance i )
{
    const int index = _GetIndex( i );
    return _data.matrix[index];
}

bxAABB bxMeshComponent_Manager::localAABB( bxMeshComponent_Instance i )
{
    const int index = _GetIndex( i );
    return _data.localAABB[index];
}

void bxMeshComponent_Manager::setLocalAABB( bxMeshComponent_Instance i, const bxAABB& aabb )
{
    const int index = _GetIndex( i );
    _data.localAABB[index] = aabb;
}


bxMeshComponent_Manager::bxMeshComponent_Manager()
    : _alloc(0)
    , _memoryHandle(0)
{
    memset( &_data, 0x00, sizeof( InstanceData ) );
}

void bxMeshComponent_Manager::_Startup(bxAllocator* alloc)
{
    SYS_ASSERT( _alloc == 0 );
    _alloc = alloc;

    _alloc_matrix._Startup( alloc );

}

void bxMeshComponent_Manager::_Shutdown()
{
    SYS_ASSERT( _data.size == 0 );

    _alloc_matrix._Shutdown();

    BX_FREE0( _alloc, _memoryHandle );
    memset( &_data, 0x00, sizeof( InstanceData ) );
    _indices.reset();
}

void bxMeshComponent_Manager::_Allocate( int newCapacity )
{
    if( newCapacity <= _data.capacity )
        return;

    int memSize = 0;
    memSize += newCapacity * sizeof( *_data.entity );
    memSize += newCapacity * sizeof( *_data.mesh );
    memSize += newCapacity * sizeof( *_data.localAABB );
    memSize += newCapacity * sizeof( *_data.matrix );

    void* mem = BX_MALLOC( _alloc, memSize, 8 );
    memset( mem, 0x00, memSize );

    InstanceData newData;
    memset( &newData, 0x00, sizeof(InstanceData) );
    bxBufferChunker chunker( mem, memSize );
    newData.entity    = chunker.add< bxEntity >( newCapacity );
    newData.mesh      = chunker.add< bxMeshComponent_Data >( newCapacity );
    newData.localAABB = chunker.add< bxAABB >( newCapacity );
    newData.matrix    = chunker.add< bxComponent_Matrix >( newCapacity );
    newData.size      = _data.size;
    newData.capacity  = newCapacity;

    chunker.check();

    if( _memoryHandle )
    {
        memcpy( newData.entity    , _data.entity    , _data.size * sizeof(*_data.entity     ) );
        memcpy( newData.mesh      , _data.mesh      , _data.size * sizeof(*_data.mesh       ) );
        memcpy( newData.localAABB , _data.localAABB , _data.size * sizeof(*_data.localAABB  ) );
        memcpy( newData.matrix    , _data.matrix    , _data.size * sizeof(*_data.matrix     ) );
        BX_FREE0( _alloc, _memoryHandle );
    }

    _data = newData;
    _memoryHandle = mem;
}

void bxMeshComponent_Manager::_Callback_releaseEntity( bxEntity e, void* userData )
{
    bxMeshComponent_Manager* _this = (bxMeshComponent_Manager*)userData;
    _this->release( e );
}

#include <gfx/gfx_render_list.h>
void bxComponent::mesh_createRenderList( bxGfxRenderList* rList, const bxMeshComponent_Manager& meshManager )
{
    const bxMeshComponent_Manager::InstanceData& data = meshManager.data();
    const int nComponents = data.size;
    for( int ic = 0; ic < nComponents; ++ic )
    {
        const bxMeshComponent_Data& meshData = data.mesh[ic];
        const bxAABB& aabb = data.localAABB[ic];
        const bxComponent_Matrix mx = data.matrix[ic];

        const int dataIndex = rList->renderDataAdd( meshData.rsource, meshData.shader, meshData.passIndex, aabb );
        const u32 surfIndex = rList->surfacesAdd( &meshData.surface, 1 );
        const u32 poseIndex = rList->instancesAdd( mx.pose, mx.n );
        rList->itemSubmit( dataIndex, surfIndex, poseIndex, meshData.mask, meshData.layer );
    }
}
