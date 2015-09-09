#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx
{
    
    struct GfxContext;
    struct GfxCommandQueue;
    void gfxStartup( GfxContext** ctx, void* hwnd, bool debugContext, bool coreContext );
    void gfxShutdown( GfxContext** ctx );

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
    void gfxCameraViewport( GfxViewport* vp, const GfxCamera& cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void gfxCameraComputeMatrices( GfxCamera* cam );
    

    struct GfxView
    {
        u32 _viewParamsBuffer;
        u32 _instanceWorldBuffer;
        u32 _instanceWorldITBuffer;

        i32 _maxInstances;

        GfxView()
            : _viewParamsBuffer( 0 )
            , _instanceWorldBuffer( 0 )
            , _instanceWorldITBuffer( 0 )
            , _maxInstances(0)
        {}
    };
    void gfxViewCreate( GfxView* view, GfxContext* ctx, int maxInstances );
    void gfxViewDestroy( GfxView* view, GfxContext* ctx );
    
    void gfxViewCameraSet( GfxView* view, const GfxCamera& camera, int rtWidth, int rtHeight );
    void gfxViewInstanceSet( GfxView* view, int nMatrices, const Matrix4* matrices );

    void gfxViewSet( GfxCommandQueue* cmdQueue, const GfxView& view );
    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxViewport& vp );



}////
