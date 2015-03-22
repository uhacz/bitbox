#pragma once

#include <util/type.h>
#include <util/debug.h>
#include <util/vectormath/vectormath.h>
#include <util/memory.h>

struct bxScene
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

    bxScene();

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
    u32 _lastGenerationId;

    void _Allocate( int newCapacity );
};

//struct bxScene_NodeId
//{
//    i16 index;
//};
//
//struct bxScene
//{
//    bxScene_NodeId create();
//    void release( bxScene_NodeId* ids );
//
//    void link( bxScene_NodeId parent, bxScene_NodeId child );
//    void unlink( bxScene_NodeId child );
//
//    bxScene_NodeId parent( bxScene_NodeId nodeId );
//    const Matrix4& localPose( bxScene_NodeId nodeId ) const;
//    const Matrix4& worldPose( bxScene_NodeId nodeId ) const;
//
//    void setLocalRotation( bxScene_NodeId nodeId, const Matrix3& rot );
//    void setLocalPosition( bxScene_NodeId nodeId, const Vector3& pos );
//    void setLocalPose( bxScene_NodeId nodeId, const Matrix4& pose );
//    void setWorldPose( bxScene_NodeId nodeId, const Matrix4& pose );
//
//public:
//    bxScene( int allocationChunkSize = 16, bxAllocator* alloc = bxDefaultAllocator() );
//
//private:
//    enum EFlag
//    {
//        eFLAG_ACTIVE = 0x1,
//    };
//
//    struct Data
//    {
//        Matrix4* localPose;
//        Matrix4* worldPose;
//        i16*     parent;
//        u8*      flag;
//
//        i32 size;
//        i32 capacity;
//        
//        i32 _freeList;
//        void* _memoryHandle;
//    };
//    
//    Data _data;
//    bxAllocator* _alloc;
//    i32 _alloc_chunkSize;
//
//    void _Allocate( int newCapacity );
//    void _Transform( const Matrix4& parentPose, bxScene_NodeId nodeId );
//};
