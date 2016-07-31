#pragma once

#include "gfx_public.h"

struct bxGdiDeviceBackend;
struct bxGdiTexture;
struct bxGdiContext;

namespace bx
{
    void gfxContextStartup( GfxContext** gfx, bxGdiDeviceBackend* dev );
    void gfxContextShutdown( GfxContext** gfx, bxGdiDeviceBackend* dev );
    void gfxContextTick( GfxContext* gfx, bxGdiDeviceBackend* dev );
    void gfxContextFrameBegin( GfxContext* gfx, bxGdiContext* gdi );
    void gfxContextFrameEnd( GfxContext* gfx, bxGdiContext* gdi );
    void gfxCommandQueueAcquire( GfxCommandQueue** cmdq, GfxContext* ctx, bxGdiContext* gdiContext );
    void gfxCommandQueueRelease( GfxCommandQueue** cmdq );

    GfxGlobalResources* gfxGlobalResourcesGet();
    bxGdiShaderFx_Instance* gfxMaterialFind( const char* name );
    
    GfxContext* gfxContextGet( GfxScene* scene );
    bxGdiContext* gfxCommandQueueGdiContextGet( GfxCommandQueue* cmdq );
    ////
    void gfxCameraCreate( GfxCamera** camera, GfxContext* ctx );
    void gfxCameraDestroy( GfxCamera** camera );

    GfxCameraParams gfxCameraParamsGet( const GfxCamera* camera );
    void gfxCameraParamsSet( GfxCamera* camera, const GfxCameraParams& params );
    float gfxCameraAspect( const GfxCamera* cam );
    float gfxCameraFov( const GfxCamera* cam );
    Vector3 gfxCameraEye( const GfxCamera* cam );
    Vector3 gfxCameraDir( const GfxCamera* cam );
    void gfxCameraViewport( GfxViewport* vp, const GfxCamera* cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void gfxCameraComputeMatrices( GfxCamera* cam );

    void gfxCameraWorldMatrixSet( GfxCamera* cam, const Matrix4& world );
    Matrix4 gfxCameraWorldMatrixGet( const GfxCamera* camera );
    Matrix4 gfxCameraViewMatrixGet( const GfxCamera* camera );

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

#include <util/bbox.h>
#include <util/containers.h>

namespace bx{
namespace gfx{
    struct View
    {
        
    };

}}////


namespace bx{
namespace gfx{

struct MeshInstance
{
    u32 i;
};

class MeshComponent
{
public:

    MeshInstance create( id_t host, bxGdiRenderSource* rsource, bxGdiShaderFx_Instance* shader, unsigned nInstances );
    void destroy( MeshInstance mi );

    MeshInstance get( id_t host );
    bool isValid( MeshInstance mi ) const { return mi.i != UINT32_MAX; }

    void renderMaskSet( MeshInstance mi, u8 mask );
    void renderLayerSet( MeshInstance mi, u8 layer );
    void cullDistanceSet( MeshInstance mi, float cullDistance );
    void localAABBSet( MeshInstance mi, const Vector3 min, const Vector3 max );
    void worldMatrixSet( MeshInstance mi, unsigned index, const Matrix4& matrix );
    void worldMatrixSet( MeshInstance mi, unsigned first, unsigned count, const Matrix4* matrices );

    void fillCommandBuckets( bxGdiContext* ctx );

private:
    struct Data
    {
        bxGdiRenderSource* rsource{ nullptr };
        bxGdiShaderFx_Instance* shader{ nullptr };
        u8* render_mask{ nullptr };
        u8* render_layer{ nullptr };
        f32* cull_distance_sqr{ nullptr };
        bxAABB* local_aabb{ nullptr };
    };
};

}}////

