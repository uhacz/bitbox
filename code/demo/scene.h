#pragma once

#include "physics.h"
#include "game.h"
#include "design_block.h"
#include "camera_manager.h"
#include "terrain.h"

#include <gfx/gfx.h>

struct bxEngine;

namespace bx{
    struct PhxScene;
}////

namespace bx
{
    struct GameScene
    {
        bx::CameraManager* cameraManager = nullptr;
        bx::DesignBlock* dblock = nullptr;
    
        bx::GfxScene* gfxScene = nullptr;
        bx::PhxScene* phxScene = nullptr;

        //bx::Character* character = nullptr;
        bx::CharacterController* cct = nullptr;
        bx::Terrain* terrain = nullptr;
        bx::CameraController cameraController;
    };

    void gameSceneStartup( GameScene* scene, bxEngine* engine );
    void gameSceneShutdown( GameScene* scene, bxEngine* engine );
}


//#include <util/vectormath/vectormath.h>
//#include <util/memory.h>

//struct bxScene_Graph
//{
//    struct Id
//    {
//        i16 index;
//    };
//
//    Id create();
//    void release( Id* ids );
//
//    void link( Id parent, Id child );
//    void unlink( Id child );
//
//    Id parent( Id nodeId );
//    const Matrix4& localPose( Id nodeId ) const;
//    const Matrix4& worldPose( Id nodeId ) const;
//
//    void setLocalRotation( Id nodeId, const Matrix3& rot );
//    void setLocalPosition( Id nodeId, const Vector3& pos );
//    void setLocalPose( Id nodeId, const Matrix4& pose );
//    void setWorldPose( Id nodeId, const Matrix4& pose );
//
//public:
//    bxScene_Graph( int allocationChunkSize = 16, bxAllocator* alloc = bxDefaultAllocator() );
//
//private:
//    enum EFlag
//    {
//        eFLAG_ACTIVE = 0x1,
//    };
//
//    struct Bucket
//    {
//        i16 begin;
//        i16 count;
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
//    u32 _flag_recompute : 1;
//
//    void _Allocate( int newCapacity );
//    void _Transform( const Matrix4& parentPose, Id nodeId );
//};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

