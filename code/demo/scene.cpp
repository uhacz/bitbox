#include "scene.h"
#include <util/buffer_utils.h>

bxScene::bxScene( int allocationChunkSize /*= 16*/, bxAllocator* alloc /*= bxDefaultAllocator() */ )
    : _alloc( alloc )
    , _alloc_chunkSize( allocationChunkSize )
    , _flag_recompute(0)
{
    memset( &_data, 0x00, sizeof(_data) );
    _data._freeList = -1;
}

void bxScene::_Allocate( int newCapacity )
{
    if( newCapacity <= _data.capacity )
        return;

    int memSize = 0;
    memSize += newCapacity * sizeof( *_data.localPose );
    memSize += newCapacity * sizeof( *_data.worldPose );
    memSize += newCapacity * sizeof( *_data.parent );
    memSize += newCapacity * sizeof( *_data.flag );

    void* mem = BX_MALLOC( _alloc, memSize, 8 );
    memset( mem, 0x00, memSize );

    Data newData;
    memset( &newData, 0x00, sizeof(Data) );
    
    bxBufferChunker chunker( mem, memSize );
    newData.localPose     = chunker.add<Matrix4>( newCapacity );
    newData.worldPose     = chunker.add<Matrix4>( newCapacity );
    newData.parent        = chunker.add<i16>( newCapacity );
    newData.flag          = chunker.add<u8>( newCapacity );
    newData.capacity = newCapacity;
    newData.size = _data.size;
    newData._freeList = _data._freeList;
    newData._memoryHandle = mem;

    chunker.check();

    if( _data._memoryHandle )
    {
        memcpy( newData.localPose     , _data.localPose     , _data.size * sizeof(*_data.localPose     ) );
        memcpy( newData.worldPose     , _data.worldPose     , _data.size * sizeof(*_data.worldPose     ) );
        memcpy( newData.parent        , _data.parent        , _data.size * sizeof(*_data.parent        ) );
        memcpy( newData.flag          , _data.flag          , _data.size * sizeof(*_data.flag          ) );
        
        BX_FREE0( _alloc, _data._memoryHandle );
    }
    _data = newData;
}


namespace
{
    inline bxScene::Id makeNodeId( int i )
    {
        bxScene::Id id = { i };
        return id;
    }
    inline i16 nodeId_indexSafe( bxScene::Id nodeId, i32 numNodesInContainer )
    {
        const i16 index = nodeId.index;
        SYS_ASSERT( index >= 0 && index < numNodesInContainer );
        return index;
    }
}///


bxScene::Id bxScene::create()
{
    int index = -1;

    if( _data.size + 1 > _data.capacity )
    {
        const int newCapacity = _data.size + 64;
        _Allocate( newCapacity );
    }

    if( _data._freeList == -1 )
    {
        index = _data.size;
    }
    else
    {
        SYS_ASSERT( _data._freeList < _data.size );
        index = _data._freeList;
        _data._freeList = _data.parent[ index ];
        SYS_ASSERT( _data._freeList == -1 || _data._freeList < _data.size );
    }

    SYS_ASSERT( _data.flag[index] == 0 );
    _data.localPose[index] = Matrix4::identity();
    _data.worldPose[index] = Matrix4::identity();
    _data.parent[index] = -1;
    SYS_ASSERT( _data.flag[index] == 0 );
    _data.flag[index] = eFLAG_ACTIVE;
    ++_data.size;
    _flag_recompute = 1;
    return makeNodeId( index );
}

void bxScene::release( bxScene::Id* id )
{
    int index = id->index;
    if ( index < 0 || index > _data.size )
        return;

    if ( !(_data.flag[index] & eFLAG_ACTIVE) )
        return;

    SYS_ASSERT( _data.size > 0 );

    const i16 parentIndex = _data.parent[index];
    unlink( id[0] );

    _data.flag[index] = 0;
    _data.parent[index] = _data._freeList;
    _data._freeList = index;
    
    --_data.size;
    _flag_recompute = 1;
}

void bxScene::link( bxScene::Id parent, bxScene::Id child )
{
    const int parentIndex = parent.index;
    const int childIndex = child.index;

    if ( parentIndex < 0 || parentIndex > _data.size )
        return;

    if ( childIndex < 0 || childIndex > _data.size )
        return;

    unlink( child );
    _data.parent[childIndex] = parentIndex;
}

void bxScene::unlink( bxScene::Id child )
{
    const int childIndex = child.index;
    if ( childIndex < 0 || childIndex > _data.size )
        return;

    _data.parent[childIndex] = -1;
}

bxScene::Id bxScene::parent( bxScene::Id nodeId )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    return makeNodeId( _data.parent[ index ] );
}

const Matrix4& bxScene::localPose( bxScene::Id nodeId ) const 
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    return _data.localPose[index];
}

const Matrix4& bxScene::worldPose( bxScene::Id nodeId ) const 
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    return _data.worldPose[index];
}


void bxScene::setLocalRotation( bxScene::Id nodeId, const Matrix3& rot )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index].setUpper3x3( rot );

    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setLocalPosition( bxScene::Id nodeId, const Vector3& pos )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index].setTranslation( pos );

    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setLocalPose( bxScene::Id nodeId, const Matrix4& pose )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index] = pose;
    
    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setWorldPose( bxScene::Id nodeId, const Matrix4& pose )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    const int parentIndex = _data.parent[index];
    const Matrix4 parentPose = (parentIndex != -1) ? ( _data.worldPose[parentIndex] ) : Matrix4::identity();
    _data.localPose[index] = inverse( parentPose ) * pose;

    _Transform( parentPose, nodeId );
}

void bxScene::_Transform( const Matrix4& parentPose, bxScene::Id nodeId )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.worldPose[index] = parentPose * _data.localPose[index];
}
