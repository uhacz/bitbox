#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/containers.h>

struct bxGfxCamera;
struct bxGdiContext;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
class bxResourceManager;

struct bxVoxel_Octree;
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
    static const int GLOBAL_GRID_SIZE = 4096;

    ////
    ////
    void octree_insert( bxVoxel_Octree* voct, const Vector3& point, size_t data );
    void octree_clear( bxVoxel_Octree* voct );
    void octree_debugDraw( bxVoxel_Octree* voct );

    ////
    ////
    bxVoxel_ObjectId object_new( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, int gridSize );
    void object_delete( bxVoxel_Manager* menago, bxVoxel_ObjectId* vobj );
    bool object_valid( bxVoxel_Manager* menago, bxVoxel_ObjectId id );
        
    bxVoxel_Octree* object_octree( bxVoxel_Manager* ctx, bxVoxel_ObjectId id );
    const Matrix4& object_pose( bxVoxel_Manager* ctx, bxVoxel_ObjectId id );
    void object_setPose( bxVoxel_Manager* ctx, bxVoxel_ObjectId id, const Matrix4& pose );
    void object_upload( bxGdiContextBackend* ctx, bxVoxel_Manager* menago, bxVoxel_ObjectId id );

    ////
    ////
    void gfx_cull( bxVoxel_Context* vctx, const bxGfxCamera& camera );
    void gfx_draw( bxGdiContext* ctx, bxVoxel_Context* vctx, const bxGfxCamera& camera );

    ////
    ////
    void util_addBox( bxVoxel_Octree* voct, int w, int h, int d, unsigned colorRGBA );
    void util_addSphere( bxVoxel_Octree* voct, int radius, unsigned colorRGBA );
    void util_addPlane( bxVoxel_Octree* voct, int w, int h, unsigned colorRGBA );
}///


