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


    struct GfxCamera
    {
        Matrix4 world;
        Matrix4 view;
        Matrix4 proj;
    };

    struct GfxView
    {
    
    };

    void gfxViewBind( GfxCommandQueue* cmdQueue, GfxView* view, const GfxCamera& camera );
    


}////
