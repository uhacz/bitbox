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
//struct bxGfx_HSunLight   { u32 h; };
//struct bxGfx_HPointLight { u32 h; };
struct bxGfx_HInstanceBuffer { u32 h; };
struct bxGfx_HMeshInstance{ u64 h; };

struct bxGfx_World;

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
    bxGfx_World* worldCreate();
    void worldRelease( bxGfx_World** h );

    bxGfx_HMeshInstance worldMeshAdd( bxGfx_World* hworld, bxGfx_HMesh hmesh, int nInstances );
    void worldMeshLocalAABBSet( bxGfx_HMeshInstance hmeshi, const Vector3& minAABB, const Vector3& maxAABB );
    void worldMeshRemove( bxGfx_HMeshInstance hmeshi );
    void worldMeshRemoveAndRelease( bxGfx_HMeshInstance* hmeshi );
    
    
    bxGfx_HMesh meshInstanceHMesh( bxGfx_HMeshInstance hmeshi );
    bxGfx_HInstanceBuffer meshInstanceHInstanceBuffer( bxGfx_HMeshInstance hmeshi );

    void worldDraw( bxGdiContext* ctx, bxGfx_World* world, const bxGfxCamera& camera );
}///

namespace bxGfxExt
{
    inline bxGdiRenderSource* meshInstanceRenderSource( bxGfx_HMeshInstance hmeshI )
    {
        bxGfx_HMesh hmesh = bxGfx::meshInstanceHMesh( hmeshI );
        return bxGfx::meshRenderSource( hmesh );
    }
}///
