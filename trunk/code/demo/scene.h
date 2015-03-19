#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/memory.h>

struct bxScene_NodeId
{
    i16 index;
};

struct bxScene
{
    bxScene_NodeId create();
    void release( bxScene_NodeId* ids );

    void link( bxScene_NodeId parent, bxScene_NodeId child );
    void unlink( bxScene_NodeId child );

    bxScene_NodeId parent( bxScene_NodeId nodeId );

    const Matrix3& localRotation( bxScene_NodeId nodeId );
    const Vector3& localPosition( bxScene_NodeId nodeId );
    const Vector3& localScale   ( bxScene_NodeId nodeId );

    const Matrix4& localPoseCalc( bxScene_NodeId nodeId );
    const Matrix4& worldPoseCalc( bxScene_NodeId nodeId );
    const Matrix4& worldPoseFetch( bxScene_NodeId nodeId );
    const Matrix4& localPoseFetch( bxScene_NodeId nodeId );

    void setLocalRotation( bxScene_NodeId nodeId );
    void setLocalPosition( bxScene_NodeId nodeId );
    void setLocalScale( bxScene_NodeId nodeId );
    void setWorldPose( bxScene_NodeId nodeId );

    void transform( const Matrix4& world );

public:
    bxScene( int allocationChunkSize = 16, bxAllocator* alloc = bxDefaultAllocator() );

private:
    enum EFlag
    {
        eFLAG_DIRTY = 0x1,
    };

    struct Data
    {
        Matrix3* localRotation;
        Vector3* localPosition;
        Vector3* localScale;
        Matrix4* localPose;
        Matrix4* worldPose;
        i16*     parent;
        i16*     firstChild;
        i16*     nextSlibling;
        u8*      flag;

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
