#pragma once

#include <util/type.h>
#include <util/debug.h>
#include <util/memory.h>

struct bxTree
{
    struct Id
    {
        u32 index : 16;
        u32 generation : 16;
    };

    Id create();
    void release( Id* id );

    void link( Id parent, Id child );
    void unlink( Id child );

    bool has( Id id ) const{
        return id.index < (u32)_data.size && id.generation == _data.generation[id.index];
    }

    Id parent( Id id ) const{
        SYS_ASSERT( has( id ) ); return _data.parent[id.index];
    }
    Id firstChild( Id id )const{
        SYS_ASSERT( has( id ) ); return _data.firstChild[id.index];
    }
    Id nextSlibling( Id id ) const{
        SYS_ASSERT( has( id ) ); return _data.nextSlibling[id.index];
    }
    uptr userData( Id id ) const {
        SYS_ASSERT( has( id ) ); return _data.userData[id.index];
    }
    void setUserData( Id id, uptr ud ){
        SYS_ASSERT( has( id ) ); _data.userData[id.index] = ud;
    }

    bxTree();

private:
    enum EFlag
    {
        eFLAG_ACTIVE = 0x1,
    };

    struct Data 
    {
        u16*    generation;
        Id*     parent;
        Id*     firstChild;
        Id*     nextSlibling;
        u8*     flag;
        uptr*   userData;
        i32 size;
        i32 capacity;

        i32 _freeList;
        void* _memoryHandle;
    };

    Data _data;
    bxAllocator* _alloc;
    i32 _alloc_chunkSize;

    void _Allocate( int newCapacity );
};