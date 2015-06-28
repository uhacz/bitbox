#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <gdi/gdi_backend_struct.h>

struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiDeviceBackend;
struct bxGdiContext;
struct bxResourceManager;

struct bxGfxCamera;

////
////
struct bxGfx_HMesh       { u32 h; };
struct bxGfx_HSunLight   { u32 h; };
struct bxGfx_HPointLight { u32 h; };
struct bxGfx_HWorld      { u32 h; };
struct bxGfx_HInstanceBuffer { u32 h; };


struct bxGfx_StreamsDesc
{
    enum { eMAX_STREAMS = 6, };

    bxGdiVertexStreamDesc vdesc[eMAX_STREAMS];
    const void* vdata[eMAX_STREAMS];
    i32 numVStreams;

    i32 idataType;
    const void* idata;


    bxGfx_StreamsDesc();
    bxGdiVertexStreamDesc& vstream_begin( const void* data = 0 );
    bxGfx_StreamsDesc& vstream_end();
    bxGfx_StreamsDesc& istream_set( bxGdi::EDataType dataType, const void* data = 0 );
};



namespace bxGfx
{
    void startup();
    void shutdown();

    bxGfx_HMesh mesh_create();
    void mesh_release( bxGfx_HMesh* h );

    int  mesh_setStreams( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, const bxGfx_StreamsDesc& sdesc );
    int  mesh_setShader ( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* shaderName );
    bxGfx_HInstanceBuffer mesh_createInstanceBuffer( bxGfx_HMesh hmesh, int nInstances );

    int instance_get( bxGfx_HInstanceBuffer hinstance, Matrix4* buffer, int bufferSize, int startIndex = 0 );
    int instance_set( bxGfx_HInstanceBuffer hinstance, const Matrix4* buffer, int bufferSize, int startIndex = 0 );


    bxGfx_HWorld world_create();
    void world_release( bxGfx_HWorld* h );

    void world_add ( bxGfx_HWorld world, bxGfx_HMesh hmesh );
    void world_draw( bxGfx_HWorld world, const bxGfxCamera& camera );
}///
