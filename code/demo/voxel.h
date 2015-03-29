#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/containers.h>

struct bxGfxCamera;
struct bxGdiContext;
struct bxGdiDeviceBackend;
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
struct bxVoxel_Object
{
    bxVoxel_Octree* octree() { return _octree;  }
    const Matrix4& pose() const { return _pose[0]; }
    void setPose( const Matrix4& pose ) { _pose[0] = pose; }

private:
    bxVoxel_Octree* _octree;
    Matrix4*        _pose;
};

namespace bxVoxel
{
    bxVoxel_Context* _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Context** vctx );

    bxVoxel_Manager* manager( bxVoxel_Context* ctx );
    bxVoxel_Gfx* gfx( bxVoxel_Context* ctx );
}

namespace bxVoxel
{
    static const int GLOBAL_GRID_SIZE = 4096;

    void octree_insert( bxVoxel_Octree* voct, const Vector3& point, size_t data );
    void octree_clear( bxVoxel_Octree* voct );
    void octree_debugDraw( bxVoxel_Octree* voct );

    bxVoxel_Object* object_new( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, int gridSize );
    void object_delete( bxVoxel_Manager* menago, bxVoxel_Object** vobj );
    
    void gfx_cull( bxVoxel_Context* vctx, const bxGfxCamera& camera );
    void gfx_draw( bxVoxel_Gfx* vgfx, bxGdiContext* ctx );
    
}///


namespace bxVoxel
{
    void util_createBox();
    void util_createSphere();
    void util_createPlane();
}///


