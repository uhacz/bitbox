#include "scene.h"
#include <util\buffer_utils.h>
#include <string.h>




bxTree::bxTree()
    : _alloc( bxDefaultAllocator() )
    , _alloc_chunkSize( 64 )
    , _lastGenerationId(0)
{
    memset( &_data, 0x00, sizeof( _data ) );
    _data._freeList = -1;
}
void bxTree::_Allocate(int newCapacity)
{
    if ( newCapacity <= _data.capacity )
        return;

    int memSize = 0;
    memSize += newCapacity * sizeof( *_data.generation );
    memSize += newCapacity * sizeof( *_data.parent );
    memSize += newCapacity * sizeof( *_data.firstChild );
    memSize += newCapacity * sizeof( *_data.nextSlibling );
    memSize += newCapacity * sizeof( *_data.flag );
    memSize += newCapacity * sizeof( *_data.userData );

    void* mem = BX_MALLOC( _alloc, memSize, 8 );
    memset( mem, 0x00, memSize );

    Data newData;
    memset( &newData, 0x00, sizeof( Data ) );

    bxBufferChunker chunker( mem, memSize );
    newData.generation = chunker.add<u16>( newCapacity );
    newData.parent = chunker.add<Id>( newCapacity );
    newData.firstChild = chunker.add<Id>( newCapacity );
    newData.nextSlibling = chunker.add<Id>( newCapacity );
    newData.flag = chunker.add<u8>( newCapacity );
    newData.userData = chunker.add<uptr>( newCapacity );
    newData.capacity = newCapacity;
    newData.size = _data.size;
    newData._freeList = _data._freeList;
    newData._memoryHandle = mem;

    chunker.check();

    if ( _data._memoryHandle )
    {
        memcpy( newData.parent, _data.parent, _data.size * sizeof( *_data.parent ) );
        memcpy( newData.firstChild, _data.firstChild, _data.size * sizeof( *_data.firstChild ) );
        memcpy( newData.nextSlibling, _data.nextSlibling, _data.size * sizeof( *_data.nextSlibling ) );
        memcpy( newData.flag, _data.flag, _data.size * sizeof( *_data.flag ) );
        memcpy( newData.userData, _data.userData, _data.size * sizeof( *_data.userData) );

        BX_FREE0( _alloc, _data._memoryHandle );
    }
    _data = newData;
}

namespace
{
    inline bxTree::Id id_makeInvalid()
    {
        bxTree::Id id;
        id.index = 0;
        id.generation = 0;
        return id;
    }
    inline bool id_isValid( bxTree::Id id )
    {
        return id.generation != 0;
    }

    inline bool operator == ( const bxTree::Id a, const bxTree::Id b )
    {
        return a.index == b.index && a.generation == b.generation;
    }
}

bxTree::Id bxTree::create()
{
    Id id = id_makeInvalid();
    if ( _data.size + 1 > _data.capacity )
    {
        const int newCapacity = _data.size + 64;
        _Allocate( newCapacity );
    }

    if ( _data._freeList == -1 )
    {
        id.index = _data.size;
        id.generation = 1;
    }
    else
    {
        SYS_ASSERT( _data._freeList < _data.size );
        id.index = _data._freeList;
        id.generation = _data.generation[id.index];
        _data._freeList = _data.parent[id.index].index;
        SYS_ASSERT( _data._freeList == -1 || _data._freeList < _data.size );
    }

    if ( id.generation == 0 )
        ++id.generation;

    const int index = id.index;
    SYS_ASSERT( _data.flag[index] == 0 );
    
    _data.generation[index] = id.generation;
    _data.parent[index] = id_makeInvalid();
    _data.firstChild[index] = id_makeInvalid();
    _data.nextSlibling[index] = id_makeInvalid();
    _data.userData[index] = 0;
    SYS_ASSERT( _data.flag[index] == 0 );
    _data.flag[index] = eFLAG_ACTIVE;
    ++_data.size;

    return id;
}

void bxTree::release( Id* id )
{
    if ( !has( id[0] ) )
        return;

    int index = id->index;
    if ( !(_data.flag[index] & eFLAG_ACTIVE) )
        return;

    SYS_ASSERT( _data.size > 0 );

    unlink( id[0] );

    const Id parentId = _data.parent[index];
    Id child = _data.firstChild[index];
    while ( id_isValid( child ) )
    {
        SYS_ASSERT( (_data.flag[child.index] & eFLAG_ACTIVE) != 0 );
        SYS_ASSERT( _data.parent[child.index] == id[0] );
        _data.parent[child.index] = parentId;
        child = _data.nextSlibling[child.index];
    }

    _data.flag[index] = 0;
    _data.parent[index].index = _data._freeList;
    _data.parent[index].generation = 0;
    ++_data.generation[index];
    _data._freeList = index;

    --_data.size;
}

void bxTree::link( Id parent, Id child )
{
    if ( !has( parent ) )
        return;

    if ( !has( child ) )
        return;

    unlink( child );

    const Id parentFirstChildId = _data.firstChild[parent.index];
    _data.firstChild[parent.index] = child;
    _data.parent[child.index] = parent;
    _data.nextSlibling[child.index] = parentFirstChildId;
}

void bxTree::unlink( Id child )
{
    if ( !has( child ) )
        return;

    const Id parentId = _data.parent[child.index];
    if ( id_isValid( parentId ) )
    {
        if ( _data.firstChild[parentId.index] == child )
        {
            _data.firstChild[parentId.index] = _data.nextSlibling[child.index];
        }
        else
        {
            Id childTmp = _data.firstChild[parentId.index];
            while ( id_isValid( childTmp ) )
            {
                if ( _data.nextSlibling[childTmp.index] == child )
                {
                    _data.nextSlibling[childTmp.index] = _data.nextSlibling[child.index];
                    break;
                }
                childTmp = _data.nextSlibling[childTmp.index];
            }
        }
    }

    _data.parent[child.index] = id_makeInvalid();
}

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
    newData.localPose     = chunker.add<Matrix4>( newCapacity );
    newData.worldPose     = chunker.add<Matrix4>( newCapacity );
    newData.parent        = chunker.add<i16>( newCapacity );
    newData.firstChild   = chunker.add<i16>( newCapacity );
    newData.nextSlibling = chunker.add<i16>( newCapacity );
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
        memcpy( newData.firstChild   , _data.firstChild   , _data.size * sizeof(*_data.firstChild   ) );
        memcpy( newData.nextSlibling , _data.nextSlibling , _data.size * sizeof(*_data.nextSlibling ) );
        memcpy( newData.flag          , _data.flag          , _data.size * sizeof(*_data.flag          ) );
        
        BX_FREE0( _alloc, _data._memoryHandle );
    }
    _data = newData;
}


namespace
{
    inline bxScene_NodeId makeNodeId( int i )
    {
        bxScene_NodeId id = { i };
        return id;
    }
    inline i16 nodeId_indexSafe( bxScene_NodeId nodeId, i32 numNodesInContainer )
    {
        const i16 index = nodeId.index;
        SYS_ASSERT( index >= 0 && index < numNodesInContainer );
        return index;
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
    _data.localPose[index] = Matrix4::identity();
    _data.worldPose[index] = Matrix4::identity();
    _data.parent[index] = -1;
    _data.firstChild[index] = -1;
    _data.nextSlibling[index] = -1;
    SYS_ASSERT( _data.flag[index] == 0 );
    _data.flag[index] = eFLAG_ACTIVE;
    ++_data.size;
    return makeNodeId( index );
}

void bxScene::release( bxScene_NodeId* id )
{
    int index = id->index;
    if ( index < 0 || index > _data.size )
        return;

    if ( !(_data.flag[index] & eFLAG_ACTIVE) )
        return;

    SYS_ASSERT( _data.size > 0 );

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
    
    --_data.size;
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

    _data.parent[childIndex] = -1;

}
bxScene_NodeId bxScene::parent( bxScene_NodeId nodeId )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    return makeNodeId( _data.parent[ index ] );
}

const Matrix4& bxScene::localPose( bxScene_NodeId nodeId ) const 
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    //if( _data.flag[index] & eFLAG_DIRTY )
    //{
    //    const Matrix3& rot   = _data.localRotation[index];
    //    const Vector3& pos   = _data.localPosition[index];
    //    const Vector3& scale = _data.localScale[index];
    //    _data.localPose[index] = appendScale( Matrix4( rot, pos ), scale );
    //    _data.flag[index] &= ~eFLAG_DIRTY;
    //}

    return _data.localPose[index];
}



const Matrix4& bxScene::worldPose( bxScene_NodeId nodeId ) const 
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    return _data.worldPose[index];
    //if( _data.flag[index] & eFLAG_DIRTY )
    //{
    //    i16 parentIdx = _data.parent[index];
    //    Matrix4 parentPose = Matrix4::identity();
    //    if ( parentIdx != -1 )
    //    {
    //        parentPose = worldPoseCalc( makeNodeId( parentIdx ) );
    //    }

    //    _data.flag[index] &= ~eFLAG_DIRTY;
    //    _data.worldPose[index] = parentPose * _data.localPose[index];
    //}
    //
    //return _data.worldPose[index];
}


void bxScene::setLocalRotation( bxScene_NodeId nodeId, const Matrix3& rot )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index].setUpper3x3( rot );

    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setLocalPosition( bxScene_NodeId nodeId, const Vector3& pos )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index].setTranslation( pos );

    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setLocalPose( bxScene_NodeId nodeId, const Matrix4& pose )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    _data.localPose[index] = pose;
    
    const int parentIndex = _data.parent[index];
    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
    _Transform( parentPose, nodeId );
}

void bxScene::setWorldPose( bxScene_NodeId nodeId, const Matrix4& pose )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );
    const int parentIndex = _data.parent[index];
    const Matrix4 parentPose = (parentIndex != -1) ? ( _data.worldPose[parentIndex] ) : Matrix4::identity();
    _data.localPose[index] = inverse( parentPose ) * pose;

    _Transform( parentPose, nodeId );
}

void bxScene::_Transform( const Matrix4& parentPose, bxScene_NodeId nodeId )
{
    const int index = nodeId_indexSafe( nodeId, _data.size );

    _data.worldPose[index] = parentPose * _data.localPose[index];

    int child = _data.firstChild[index];
    while( child != -1 )
    {
        _Transform( _data.worldPose[index], makeNodeId( child ) );
        child = _data.nextSlibling[child];
    }
}
