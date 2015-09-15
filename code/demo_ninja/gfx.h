#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <resource_manager/resource_manager.h>

namespace bx
{
    
    struct GfxContext;
    struct GfxCommandQueue;
    void gfxStartup( GfxContext** ctx, void* hwnd, bool debugContext, bool coreContext );
    void gfxShutdown( GfxContext** ctx );
    
    GfxCommandQueue* gfxAcquireCommandQueue( GfxContext* ctx );
    void gfxReleaseCommandQueue( GfxCommandQueue** cmdQueue );

    void gfxFrameBegin( GfxContext* ctx, GfxCommandQueue* cmdQueue );
    void gfxFrameEnd( GfxContext* ctx );

}///

namespace bx
{
    struct GfxCamera
    {
        Matrix4 world;
        Matrix4 view;
        Matrix4 proj;
        Matrix4 viewProj;

        f32 hAperture;
        f32 vAperture;
        f32 focalLength;
        f32 zNear;
        f32 zFar;
        f32 orthoWidth;
        f32 orthoHeight;

        GfxCamera();
    };
    
    struct GfxViewport
    {
        i16 x, y;
        u16 w, h;
        GfxViewport() {}
        GfxViewport( int xx, int yy, unsigned ww, unsigned hh )
            : x( xx ), y( yy ), w( ww ), h( hh ) {}
    };

    float gfxCameraAspect( const GfxCamera& cam );
    float gfxCameraFov( const GfxCamera& cam );
    Vector3 gfxCameraEye( const GfxCamera& cam );
    Vector3 gfxCameraDir( const GfxCamera& cam );
    void gfxCameraViewport( GfxViewport* vp, const GfxCamera& cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void gfxCameraComputeMatrices( GfxCamera* cam );
    
    void gfxCameraSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera );
    void gfxInstanceSet( GfxCommandQueue* cmdQueue, int nMatrices, const Matrix4* matrices );
    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxViewport& vp );
    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera );
}////

namespace bx
{
    struct GfxShaderId { u32 hash; };
    GfxShaderId gfxShaderCreate( GfxContext* ctx, bxResourceManager* resourceManager );
    void gfxShaderDestroy( GfxShaderId* id, GfxContext* ctx, bxResourceManager* resourceManager );
    void gfxShaderEnable( GfxCommandQueue* ctx, GfxShaderId id );
    
    struct GfxMeshId { u32 hash; };
    GfxMeshId gfxMeshCreate( GfxContext* ctx );
    void gfxMeshDestroy( GfxMeshId* id, GfxContext* ctx );
    void gfxMeshLoadBox( GfxMeshId id, GfxContext* ctx );
    void gfxMeshLoadSphere( GfxMeshId id, GfxContext* ctx );
    

    //struct GfxLinesContext;
    //void gfxLinesContextCreate( GfxLinesContext** linesCtx, GfxContext* ctx, bxResourceManager* resourceManager );
    //void gfxLinesContextDestroy( GfxLinesContext** linesCtx, GfxContext* ctx );

    struct GfxLinesData;
    void gfxLinesDataCreate( GfxLinesData** lines, GfxContext* ctx, int initialCapacity );
    void gfxLinesDataDestroy( GfxLinesData** lines, GfxContext* ctx );

    void gfxLinesDataAdd( GfxLinesData* lines, int count, const Vector3* points, const Vector3* normals, const u32* colors );
    void gfxLinesDataClear( GfxLinesData* lines );
    void gfxLinesDataUpload( GfxCommandQueue* cmdQueue, const GfxLinesData* lines );
    void gfxLinesDataFlush( GfxCommandQueue* cmdQueue, GfxLinesData* lines );

    namespace util
    {
        void gfxLinesAddBox( GfxLinesData* lines, const Matrix4& pose, const Vector3& ext, u32 color );
        void gfxLinesAddAxes( GfxLinesData* lines, const Matrix4& pose, int colorScheme );
    }///

}///
