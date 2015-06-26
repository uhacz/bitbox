#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

struct bxRenderer_HMesh       { u32 h; };
struct bxRenderer_HTransform  { u32 h; };
struct bxRenderer_HSunLight   { u32 h; };
struct bxRenderer_HPointLight { u32 h; };
struct bxRenderer_HWorld      { u32 h; };

struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiDeviceBackend;
struct bxGdiContext;
struct bxResourceManager;

struct bxGfxCamera;

////
////
namespace bxRenderer
{
    void startup();
    void shutdown();

    bxRenderer_HMesh mesh_create();
    void mesh_release( bxRenderer_HMesh* h );
    int  mesh_setStreams( bxRenderer_HMesh hmesh, bxGdiRenderSource* rsource );
    int  mesh_setShader( bxRenderer_HMesh hmesh, bxGdiShaderFx_Instance* fxI );

    bxRenderer_HTransform transform_create();
    void transform_release( bxRenderer_HTransform* h );
    void transform_init( bxRenderer_HTransform htransform, int numPoses );
    void transform_get( bxRenderer_HTransform htransform, Matrix4* buffer, int bufferSize, int startIndex = 0 );
    void transform_set( bxRenderer_HTransform htransform, const Matrix4* buffer, int bufferSize, int startIndex = 0 );

    bxRenderer_HWorld world_create();
    void world_release( bxRenderer_HWorld* h );

    void world_add( bxRenderer_HWorld world, bxRenderer_HMesh hmesh, bxRenderer_HTransform htransform );
    void world_gc( bxRenderer_HWorld world );

    void world_draw( bxRenderer_HWorld world, const bxGfxCamera& camera );
}///
