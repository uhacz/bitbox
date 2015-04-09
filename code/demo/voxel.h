#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/bbox.h>
#include <util/containers.h>

struct bxGfxCamera;
struct bxGdiContext;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
class bxResourceManager;


struct bxVoxel_Octree;
struct bxVoxel_Map;
struct bxVoxel_ObjectId;
struct bxVoxel_Manager;
struct bxVoxel_Gfx;
struct bxVoxel_Context;

struct bxVoxel_GpuData
{
    u32 gridIndex;
    u32 colorRGBA;
};

struct bxVoxel_ObjectId
{
    u32 index : 16;
    u32 generation : 16;
};

namespace bxVoxel
{
    bxVoxel_Context* _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void _Shutdown( bxGdiDeviceBackend* dev, bxVoxel_Context** vctx );

    bxVoxel_Manager* manager( bxVoxel_Context* ctx );
    bxVoxel_Gfx* gfx( bxVoxel_Context* ctx );
}

namespace bxVoxel
{
    ////
    ////
    //void octree_insert( bxVoxel_Octree* voct, const Vector3& point, size_t data );
    //void octree_clear( bxVoxel_Octree* voct );
    //void octree_build( bxVoxel_Octree* voct, const bxVoxel_Map* m );
    //void octree_debugDraw( bxVoxel_Octree* voct );

    void map_insert( bxVoxel_Map* m, const Vector3& point, size_t data );
    void map_clear( bxVoxel_Map* m );

    void gpu_uploadShell( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, bxVoxel_ObjectId id );
    
    ////
    ////
    bxVoxel_ObjectId object_new( bxVoxel_Manager* menago );
    void object_delete( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, bxVoxel_ObjectId* vobj );
    bool object_valid( bxVoxel_Manager* menago, bxVoxel_ObjectId id );
    int object_setAttribute( bxVoxel_Manager* menago, bxVoxel_ObjectId id, const char* attrName, const void* attrData, unsigned attrDataSize );
        
    //bxVoxel_Octree* object_octree( bxVoxel_Manager* ctx, bxVoxel_ObjectId id );
    bxVoxel_Map* object_map( bxVoxel_Manager* menago, bxVoxel_ObjectId id );

    const bxAABB& object_aabb( bxVoxel_Manager* menago, bxVoxel_ObjectId id ); 
    const Matrix4& object_pose( bxVoxel_Manager* menago, bxVoxel_ObjectId id );
    
    void object_setPose( bxVoxel_Manager* menago, bxVoxel_ObjectId id, const Matrix4& pose );
    void object_setAABB( bxVoxel_Manager* menago, bxVoxel_ObjectId id, const bxAABB& aabb );

    ////
    ////
    void gfx_cull( bxVoxel_Context* vctx, const bxGfxCamera& camera );
    void gfx_draw( bxGdiContext* ctx, bxVoxel_Context* vctx, const bxGfxCamera& camera );

    ////
    ////
    void util_addBox( bxVoxel_Map* voct, int w, int h, int d, unsigned colorRGBA );
    void util_addSphere( bxVoxel_Map* voct, int radius, unsigned colorRGBA );
    void util_addPlane( bxVoxel_Map* voct, int w, int h, unsigned colorRGBA );
}///


