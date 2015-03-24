#include "tree.h"

#include <util\buffer_utils.h>
#include <string.h>

bxTree::bxTree()
    : _alloc( bxDefaultAllocator() )
    , _alloc_chunkSize( 64 )
{
    memset( &_data, 0x00, sizeof( _data ) );
    _data._freeList = -1;
}
void bxTree::_Allocate( int newCapacity )
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
        memcpy( newData.userData, _data.userData, _data.size * sizeof( *_data.userData ) );

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

    inline bool operator == (const bxTree::Id a, const bxTree::Id b)
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
