#pragma once

#include "gfx_public.h"

struct bxGdiDeviceBackend;
struct bxGdiTexture;
struct bxGdiContext;
class bxResourceManager;

namespace bx
{
    void gfxContextStartup( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void gfxContextShutdown( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void gfxContextTick( GfxContext* gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void gfxContextFrameBegin( GfxContext* gfx, bxGdiContext* gdi );
    void gfxContextFrameEnd( GfxContext* gfx, bxGdiContext* gdi );
    void gfxCommandQueueAcquire( GfxCommandQueue** cmdq, GfxContext* ctx, bxGdiContext* gdiContext );
    void gfxCommandQueueRelease( GfxCommandQueue** cmdq );

    void gfxSubmitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName );
    void gfxCopyTextureRGBA( bxGdiContext* ctx, const bxGdiTexture& outputTexture, const bxGdiTexture& inputTexture );
    void gfxRasterizeFramebuffer( bxGdiContext* ctx, const bxGdiTexture& colorFB, float cameraAspect );

    GfxGlobalResources* gfxGlobalResourcesGet();
    bxGdiShaderFx_Instance* gfxMaterialFind( const char* name );

    ////
    void gfxCameraCreate( GfxCamera** camera, GfxContext* ctx );
    void gfxCameraDestroy( GfxCamera** camera );
    float gfxCameraAspect( const GfxCamera* cam );
    float gfxCameraFov( const GfxCamera* cam );
    Vector3 gfxCameraEye( const GfxCamera* cam );
    Vector3 gfxCameraDir( const GfxCamera* cam );
    void gfxCameraViewport( GfxViewport* vp, const GfxCamera* cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void gfxCameraComputeMatrices( GfxCamera* cam );

    void gfxCameraWorldMatrixSet( GfxCamera* cam, const Matrix4& world );
    Matrix4 gfxCameraWorldMatrixGet( const GfxCamera* camera );

    ////
    void gfxSunLightDirectionSet( GfxContext* ctx, const Vector3& direction );
    
    ////
    void gfxMeshInstanceCreate( GfxMeshInstance** meshI, GfxContext* ctx, int numInstances = 1 );
    void gfxMeshInstanceDestroy( GfxMeshInstance** meshI );

    bxGdiRenderSource* gfxMeshInstanceRenderSourceGet( GfxMeshInstance* meshI );
    bxGdiShaderFx_Instance* gfxMeshInstanceFxGet( GfxMeshInstance* meshI );

    void gfxMeshInstanceDataSet( GfxMeshInstance* meshI, const GfxMeshInstanceData& data );
    void gfxMeshInstanceWorldMatrixSet( GfxMeshInstance* meshI, const Matrix4* matrices, int nMatrices );

    ////
    void gfxSceneCreate( GfxScene** scene, GfxContext* ctx );
    void gfxSceneDestroy( GfxScene** scene );
    
    void gfxSceneMeshInstanceAdd( GfxScene* scene, GfxMeshInstance* meshI );
    void gfxSceneMeshInstanceRemove( GfxScene* scene, GfxMeshInstance* meshI );

    void gfxSceneDraw( GfxScene* scene, GfxCommandQueue* cmdq, const GfxCamera* camera );
    

}///



