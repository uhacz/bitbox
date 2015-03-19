#include "scene.h"
#include <util\buffer_utils.h>
#include <string.h>

bxScene::bxScene( int allocationChunkSize /*= 16*/, bxAllocator* alloc /*= bxDefaultAllocator() */ )
    : _alloc( alloc )
    , _alloc_chunkSize( allocationChunkSize )
{
    memset( &_data, 0x00, sizeof(_data) );
    _data._freeList = -1;
}

void bxScene::_Allocate( int newCapacity )
{
    if( newCapacity <= _data.capacity )
        return;

    int memSize = 0;
    memSize += newCapacity * sizeof( *_data.localRotation );
    memSize += newCapacity * sizeof( *_data.localPosition );
    memSize += newCapacity * sizeof( *_data.localScale );
    memSize += newCapacity * sizeof( *_data.localPose );
    memSize += newCapacity * sizeof( *_data.worldPose );
    memSize += newCapacity * sizeof( *_data.parent );
    memSize += newCapacity * sizeof( *_data.firstChild );
    memSize += newCapacity * sizeof( *_data.nextSlibling );
    memSize += newCapacity * sizeof( *_data.flag );

    void* mem = BX_MALLOC( _alloc, memSize, 8 );
    memset( mem, 0x00, memSize );

    Data newData;
    memset( &newData, 0x00, sizeof(Data) );
    bxBufferChunker chunker( mem, memSize );
    newData.localRotation = chunker.add<Matrix3>( newCapacity );
    newData.localPosition = chunker.add<Vector3>( newCapacity );
    newData.localScale    = chunker.add<Vector3>( newCapacity );
    newData.localPose     = chunker.add<Matrix4>( newCapacity );
    newData.worldPose     = chunker.add<Matrix4>( newCapacity );
    newData.parent        = chunker.add<i16>( newCapacity );
    newData.firstChild   = chunker.add<i16>( newCapacity );
    newData.nextSlibling = chunker.add<i16>( newCapacity );
    newData.flag          = chunker.add<u8>( newCapacity );
    newData.capacity = newCapacity;
    newData.size = _data.size;
    newData._memoryHandle = mem;

    chunker.check();

    if( _data._memoryHandle )
    {
        memcpy( newData.localRotation , _data.localRotation , _data.size * sizeof(*_data.localRotation ) );
        memcpy( newData.localPosition , _data.localPosition , _data.size * sizeof(*_data.localPosition ) );
        memcpy( newData.localScale    , _data.localScale    , _data.size * sizeof(*_data.localScale    ) );
        memcpy( newData.localPose     , _data.localPose     , _data.size * sizeof(*_data.localPose     ) );
        memcpy( newData.worldPose     , _data.worldPose     , _data.size * sizeof(*_data.worldPose     ) );
        memcpy( newData.parent        , _data.parent        , _data.size * sizeof(*_data.parent        ) );
        memcpy( newData.firstChild   , _data.firstChild   , _data.size * sizeof(*_data.firstChild   ) );
        memcpy( newData.nextSlibling , _data.nextSlibling , _data.size * sizeof(*_data.nextSlibling ) );
        memcpy( newData.flag          , _data.flag          , _data.size * sizeof(*_data.flag          ) );
        
        BX_FREE0( _alloc, _data._memoryHandle );
    }
    _data = newData;
}

namespace
{
    bxScene_NodeId makeNodeId( int i )
    {
        bxScene_NodeId id = { i };
        return id;
    }
}///

bxScene_NodeId bxScene::create()
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
    _data.localRotation[index] = Matrix3::identity();
    _data.localPosition[index] = Vector3(0.f);
    _data.localScale[index] = Vector3(1.f);
    _data.localPose[index] = Matrix4::identity();
    _data.worldPose[index] = Matrix4::identity();
    _data.parent[index] = -1;
    _data.firstChild[index] = -1;
    _data.nextSlibling[index] = -1;
    _data.flag[index] = eFLAG_ACTIVE;

    return makeNodeId( index );
}

void bxScene::release( bxScene_NodeId* id )
{
    int index = id->index;
    if ( index < 0 || index > _data.size )
        return;

    if ( !(_data.flag[index] & eFLAG_ACTIVE) )
        return;

    const i16 parentIndex = _data.parent[index];
    unlink( id[0] );

    i16 child = _data.firstChild[index];
    while( child != -1 )
    {
        SYS_ASSERT( ( _data.flag[child] & eFLAG_ACTIVE ) != 0 );
        SYS_ASSERT( _data.parent[child] == index );
        _data.parent[child] = parentIndex;
        child = _data.nextSlibling[child];
    }

    _data.flag[index] = 0;
    _data.parent[index] = _data._freeList;
    _data._freeList = index;

}

void bxScene::link( bxScene_NodeId parent, bxScene_NodeId child )
{
    const int parentIndex = parent.index;
    const int childIndex = child.index;

    if ( parentIndex < 0 || parentIndex > _data.size )
        return;

    if ( childIndex < 0 || childIndex > _data.size )
        return;

    unlink( child );

    const i16 parentFirstChildIndex = _data.firstChild[parentIndex];
    _data.firstChild[parentIndex] = childIndex;
    _data.parent[childIndex] = parentIndex;
    _data.nextSlibling[childIndex] = parentFirstChildIndex;
}

void bxScene::unlink( bxScene_NodeId child )
{
    const int childIndex = child.index;
    if ( childIndex < 0 || childIndex > _data.size )
        return;

    const i16 parentIndex = _data.parent[childIndex];
    if( parentIndex != -1 )
    {
        if( _data.firstChild[parentIndex] == childIndex )
        {
            _data.firstChild[parentIndex] = _data.nextSlibling[childIndex];
        }
        else
        {
            i16 child = _data.firstChild[parentIndex];
            while( child != -1 )
            {
                if( _data.nextSlibling[child] == childIndex )
                {
                    _data.nextSlibling[child] = _data.nextSlibling[childIndex];
                    break;
                }
                child = _data.nextSlibling[child];
            }
        }
    }

}
