#pragma once

#include "gdi_backend.h"
#include <util/array.h>

struct bxGdiRenderSurface
{
    u32 topology;
    u32 begin;
    u32 count;

    bxGdiRenderSurface()
        : topology(0)
        , begin(0), count(0)
    {}

    bxGdiRenderSurface( int topo, int b, int cnt )
        : topology(topo), begin(b), count(cnt)
    {}
};

struct bxGdiRenderSource
{
    i32 numVertexBuffers;
    bxGdiIndexBuffer indexBuffer;
    bxGdiVertexBuffer vertexBuffers[1];

    bxGdiRenderSource()
        : numVertexBuffers(0)
    {}
};

namespace bxGdi
{
    bxGdiRenderSource* renderSource_new( int numStreams, bxAllocator* allocator = bxDefaultAllocator() );
    void renderSource_free( bxGdiRenderSource** rsource, bxAllocator* allocator = bxDefaultAllocator() );
    void renderSource_release( bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource );
    inline void renderSource_releaseAndFree( bxGdiDeviceBackend* dev, bxGdiRenderSource** rsource, bxAllocator* allocator = bxDefaultAllocator() )
    {
        renderSource_release( dev, rsource[0] );
        renderSource_free( rsource, allocator );
    }


    void renderSource_setVertexBuffer( bxGdiRenderSource* rsource, bxGdiVertexBuffer vBuffer, int idx );
    void renderSource_setIndexBuffer( bxGdiRenderSource* rsource, bxGdiIndexBuffer iBuffer );
}///

