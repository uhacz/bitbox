#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <gdi/gdi_backend_struct.h>
#include "entity.h"

struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiDeviceBackend;
struct bxGdiContext;
class bxResourceManager;

struct bxGfxCamera;

////
//// zero means invalid handle
struct bxGfx_HMesh       { u32 h; };
struct bxGfx_HSunLight   { u32 h; };
struct bxGfx_HPointLight { u32 h; };
struct bxGfx_HWorld      { u32 h; };
struct bxGfx_HInstanceBuffer { u32 h; };
struct bxGfx_HMeshInstance{ u64 h; };

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
    bxGdiVertexStreamDesc& vstreamBegin( const void* data = 0 );
    bxGfx_StreamsDesc& vstreamEnd();
    bxGfx_StreamsDesc& istreamSet( bxGdi::EDataType dataType, const void* data = 0 );
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
    bxGfx_HMesh meshCreate();
    void meshRelease( bxGfx_HMesh* h );

    int  meshStreamsSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, const bxGfx_StreamsDesc& sdesc );
    int  meshStreamsSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource );
    
    bxGdiShaderFx_Instance* meshShaderSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* shaderName );
    int  meshShaderSet( bxGfx_HMesh hmesh, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGdiShaderFx_Instance* fxI );

    bxGdiRenderSource* meshRenderSource( bxGfx_HMesh hmesh );
    bxGdiShaderFx_Instance* meshShader( bxGfx_HMesh hmesh );

    ////
    //
    bxGfx_HInstanceBuffer instanceBuffeCreate( int nInstances );
    void instanceBufferRelease( bxGfx_HInstanceBuffer* hinstance );

    int instanceBufferData( bxGfx_HInstanceBuffer hinstance, Matrix4* buffer, int bufferSize, int startIndex = 0 );
    int instanceBufferDataSet( bxGfx_HInstanceBuffer hinstance, const Matrix4* buffer, int bufferSize, int startIndex = 0 );

    ////
    //
    bxGfx_HWorld worldCreate();
    void worldRelease( bxGfx_HWorld* h );

    bxGfx_HMeshInstance worldMeshAdd( bxGfx_HWorld hworld, bxGfx_HMesh hmesh, bxGfx_HInstanceBuffer hinstance );
    void worldMeshRemove( bxGfx_HMeshInstance hmeshi );
    void worldMeshRemoveAndRelease( bxGfx_HMeshInstance* hmeshi );
    void meshInstance( bxGfx_HMesh* hmesh, bxGfx_HInstanceBuffer* hinstance, bxGfx_HMeshInstance hmeshi );

    void worldDraw( bxGdiContext* ctx, bxGfx_HWorld hworld, const bxGfxCamera& camera );

    //void world_meshRemove( bxGfx_HWorld hworld, bxEntity_Id eid );
    //void world_meshRemoveAndRelease( bxGfx_HWorld hworld );
    //bxGfx_MeshInstance world_lookupMesh( bxGfx_HWorld hworld, bxEntity_Id eid );
}///

namespace bxGfxExt
{
    inline bxGdiRenderSource* meshInstanceRenderSource( bxGfx_HMeshInstance hmeshI )
    {
        bxGfx_HMesh hmesh;
        bxGfx_HInstanceBuffer hinst;
        bxGfx::meshInstance( &hmesh, &hinst, hmeshI );
        return bxGfx::meshRenderSource( hmesh );
    }
}///
