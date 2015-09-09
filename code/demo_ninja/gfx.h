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
    void gfxContextLoadResources( GfxContext* ctx, bxResourceManager* resourceManager );
    void gfxContextUnloadResources( GfxContext* ctx, bxResourceManager* resourceManager );

    GfxCommandQueue* gfxAcquireCommandQueue( GfxContext* ctx );
    void gfxReleaseCommandQueue( GfxCommandQueue** cmdQueue );

    struct GfxLines;
    void gfxLinesCreate( GfxLines** lines, int initialCapacity );
    void gfxLinesDestroy();

    void gfxLinesAdd( GfxLines* lines, int count, const Vector3* points, const Vector3* normals, const u32* colors );
    void gfxLinesFlush( GfxCommandQueue* cmdQueue, GfxLines* lines );

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
    
    void gfxCameraSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera, int rtWidth, int rtHeight );
    void gfxInstanceSet( GfxCommandQueue* cmdQueue, int nMatrices, const Matrix4* matrices );
    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxViewport& vp );



}////
