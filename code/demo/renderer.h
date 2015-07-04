#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <gdi/gdi_backend_struct.h>

struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiDeviceBackend;
struct bxGdiContext;
class bxResourceManager;

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

    i32 numVertices;
    i32 numIndices;
    ////
    ////
    bxGfx_StreamsDesc( int numVerts, int numIndis = 0 );
    bxGdiVertexStreamDesc& vstream_begin( const void* data = 0 );
    bxGfx_StreamsDesc& vstream_end();
    bxGfx_StreamsDesc& istream_set( bxGdi::EDataType dataType, const void* data = 0 );
};

////
////
namespace bxGfx
{
    void startup( bxGdiDeviceBackend* dev );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void frameBegin( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    ////
    //
    bxGfx_HMesh mesh_create();
    void mesh_release( bxGfx_HMesh* h );

    int  mesh_setStreams( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, const bxGfx_StreamsDesc& sdesc );
    int  mesh_setStreams( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource );
    
    int  mesh_setShader( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* shaderName );
    int  mesh_setShader( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGdiShaderFx_Instance* fxI );

    ////
    //
    bxGfx_HInstanceBuffer instanceBuffer_create( int nInstances );
    void instanceBuffer_release( bxGfx_HInstanceBuffer* hinstance );

    int instanceBuffer_get( bxGfx_HInstanceBuffer hinstance, Matrix4* buffer, int bufferSize, int startIndex = 0 );
    int instanceBuffer_set( bxGfx_HInstanceBuffer hinstance, const Matrix4* buffer, int bufferSize, int startIndex = 0 );

    ////
    //
    bxGfx_HWorld world_create();
    void world_release( bxGfx_HWorld* h );

    void world_meshAdd ( bxGfx_HWorld hworld, bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance );
    void world_draw( bxGdiContext* ctx, bxGfx_HWorld hworld, const bxGfxCamera& camera );
}///
