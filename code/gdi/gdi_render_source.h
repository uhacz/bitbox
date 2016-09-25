#pragma once

#include "gdi_backend.h"
#include <util/array.h>
#include <util/poly/poly_shape.h>

struct bxGdiContext;
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
    u32 sortHash;
    i32 numVertexBuffers;
    bxGdiIndexBuffer indexBuffer;
    bxGdiVertexBuffer vertexBuffers[1];

    bxGdiRenderSource()
        : sortHash(0)
        , numVertexBuffers(0)
    {}
};

namespace bx{
namespace gdi{

    int renderSource_memorySizeCompute( int numStreams );
    bxGdiRenderSource* renderSource_new( int numStreams, bxAllocator* allocator = bxDefaultAllocator() );
    void renderSource_free( bxGdiRenderSource** rsource, bxAllocator* allocator = bxDefaultAllocator() );
    void renderSource_release( bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource );
    inline void renderSource_releaseAndFree( bxGdiDeviceBackend* dev, bxGdiRenderSource** rsource, bxAllocator* allocator = bxDefaultAllocator() )
    {
        renderSource_release( dev, rsource[0] );
        renderSource_free( rsource, allocator );
    }
    bxGdiRenderSource* renderSource_createFromPolyShape( bxGdiDeviceBackend* dev, const bxPolyShape& shape );

    void renderSource_setVertexBuffer( bxGdiRenderSource* rsource, bxGdiVertexBuffer vBuffer, int idx );
    void renderSource_setIndexBuffer( bxGdiRenderSource* rsource, bxGdiIndexBuffer iBuffer );

    inline const bxGdiVertexBuffer& renderSourceVertexBuffer( bxGdiRenderSource* rsource, int idx )
    {
        SYS_ASSERT( idx < rsource->numVertexBuffers );
        return rsource->vertexBuffers[idx];
    }


    void renderSource_enable( bxGdiContext* ctx, bxGdiRenderSource* rsource );
    void renderSurface_draw( bxGdiContext* ctx, const bxGdiRenderSurface& surf );
    void renderSurface_drawIndexed( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int baseVertex = 0 );
    
    void renderSurface_drawInstanced( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int numInstances );
    void renderSurface_drawIndexedInstanced( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int numInstances, int baseVertex = 0 );

    inline bxGdiRenderSurface renderSource_surface( const bxGdiRenderSource* rsource, int topology )
    {
        bxGdiRenderSurface surf;
        surf.topology = topology;
        surf.begin = 0;
        surf.count = ( rsource->indexBuffer.id ) ? rsource->indexBuffer.numElements : rsource->vertexBuffers[0].numElements;
        return surf;
    }
}}///

