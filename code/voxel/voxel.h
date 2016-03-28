#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/bbox.h>

//#include <util/bbox.h>
//#include "voxel_type.h"
//
//struct bxGfxCamera;
//struct bxGdiContext;
//struct bxGdiDeviceBackend;
//struct bxGdiContextBackend;
//class bxResourceManager;
//
//namespace bxVoxel
//{
//    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
//    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
//
//    bxVoxel_Container* container_new();
//    void container_load( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Container* cnt );
//    void container_unload( bxGdiDeviceBackend* dev, bxVoxel_Container* cnt );
//    void container_delete( bxVoxel_Container** cnt );
//}
//
//namespace bxVoxel
//{
//    ////
//    ////
//    //void octree_insert( bxVoxel_Octree* voct, const Vector3& point, size_t data );
//    //void octree_clear( bxVoxel_Octree* voct );
//    //void octree_build( bxVoxel_Octree* voct, const bxVoxel_Map* m );
//    //void octree_debugDraw( bxVoxel_Octree* voct );
//    //bxVoxel_Octree* object_octree( bxVoxel_Manager* ctx, bxVoxel_ObjectId id );
//
//    void map_insert( bxVoxel_Map* m, const Vector3& point, size_t data );
//    void map_clear( bxVoxel_Map* m );
//    void gpu_uploadShell( bxGdiDeviceBackend* dev, bxVoxel_Container* container, bxVoxel_ObjectId id );
//    //TODO: void gpu_uploadVolume();
//    
//    ////
//    ////
//    bxVoxel_ObjectId object_new( bxVoxel_Container* container, const char* name );
//    bxVoxel_ObjectId object_find( const bxVoxel_Container* container, const char* name );
//    void object_delete( bxGdiDeviceBackend* dev, bxVoxel_Container* container, bxVoxel_ObjectId* vobj );
//    bool object_valid( bxVoxel_Container* container, bxVoxel_ObjectId id );
//    int object_setAttribute( bxVoxel_Container* container, bxVoxel_ObjectId id, const char* attrName, const void* attrData, unsigned attrDataSize );
//        
//    
//    bxVoxel_Map* object_map( bxVoxel_Container* container, bxVoxel_ObjectId id );
//    const bxAABB& object_aabb( bxVoxel_Container* container, bxVoxel_ObjectId id ); 
//    const Matrix4& object_pose( bxVoxel_Container* container, bxVoxel_ObjectId id );
//    
//    void object_setPose( bxVoxel_Container* container, bxVoxel_ObjectId id, const Matrix4& pose );
//    void object_setAABB( bxVoxel_Container* container, bxVoxel_ObjectId id, const bxAABB& aabb );
//
//    ////
//    ////
//    //void gfx_cull( bxVoxel_Container* vxcnt, const bxGfxCamera& camera );
//    //void gfx_draw( bxGdiContext* ctx, bxVoxel_Container* container, const bxGfxCamera& camera );
//
//    ////
//    ////
//    void util_addBox( bxVoxel_Map* voct, int w, int h, int d, unsigned char colorIndex );
//    void util_addSphere( bxVoxel_Map* voct, int radius, unsigned char colorIndex );
//    void util_addPlane( bxVoxel_Map* voct, int w, int h, unsigned char colorIndex );
//}///

namespace bx
{
    struct Octree;
    struct OctreeNodeData
    {
        uptr value;
        bool empty() const { return value == UINT64_MAX; }
    };
    
    void octreeCreate( Octree** octPtr, float size );
    void octreeDestroy( Octree** octPtr );

    int octreePointInsert( Octree* oct, const Vector3 pos, uptr data );
    OctreeNodeData octreeDataGet( const Octree* oct, int nodeIndex );
    OctreeNodeData octreeDataLookup( const Octree* oct, const Vector3 pos );
    Vector4 octreeNodePosSize( const Octree* oct, int nodeIndex );
    bxAABB octreeNodeAABB( const Octree* oct, int nodeIndex );
    int octreeRaycast( const Octree* oct, const Vector3 ro, const Vector3 rd, const floatInVec rayLength = floatInVec( FLT_MAX ) );
    void octreeDebugDraw( Octree* oct, u32 color0 = 0x00FF00FF, u32 color1 = 0xFF0000FF );
}///

namespace bx
{
    struct VoxelId
    {
        u32 id = 0;
    };
    struct VoxelContainer;
    struct VoxelRenderer;

    void voxelContainerCreate( VoxelContainer** vc );
    void voxelContainerDestroy( VoxelContainer** vc );

    VoxelId voxelCreate( VoxelContainer* vc );
    void voxelDestroy( VoxelId* id );

    void voxelRendererCreate( VoxelRenderer** vr );
    void voxelRendererDestroy( VoxelRenderer** vr );

}///