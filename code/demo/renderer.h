#pragma once

struct bxRenderer_HMesh;
struct bxRenderer_HTransform;
struct bxRenderer_HShader;
struct bxRenderer_HSunLight;
struct bxRenderer_HLight;

struct bxRenderer_World;

struct bxGdiVertexBuffer;
struct bxGdiIndexBuffer;
struct bxGdiDeviceBackend;
struct bxGdiContext;
struct bxResourceManager;

struct bxGfxCamera;

////
////
struct bxRenderer_DescMesh
{
    enum { eMAX_BUFFERS = 6, };
    
    i32 numVertices;
    i32 numVDescs;
    bxGdiVertexStreamDesc vdesc[eMAX_BUFFERS];
    const void* vdata[eMAX_BUFFERS];
    struct  
    {
        i32 dataType;
        u32 numElements;
        const void* data;
    } idesc;
};
namespace bxRenderer
{
    int descMesh_addVDesc( bxRenderer_DescMesh* desc, const bxGdiVertexStreamDesc& vdesc, const void* data );
}

namespace bxRenderer
{
    bxRenderer_HMesh mesh_create();
    void mesh_release( bxRenderer_HMesh* h );
    int  mesh_init   ( bxRenderer_HMesh hmesh, bxGdiDeviceBackend* dev, const bxRenderer_DescMesh& desc );
    void mesh_deinit ( bxRenderer_HMesh hmesh, bxGdiDeviceBackend* dev );

    bxRenderer_HShader shader_create();
    void shader_release( bxRenderer_HShader* h );
    int  shader_init   ( bxRenderer_HShader hsh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* onlyName );
    void shader_deinit ( bxRenderer_HShader hsh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void world_add( bxRenderer_World* world, bxRenderer_HMesh hmesh, bxRenderer_HShader hshader, bxRenderer_HTransform htransform );
    void world_gc( bxRenderer_World* world );

    void world_draw( bxRenderer_World* world, const bxGfxCamera& camera );
}///