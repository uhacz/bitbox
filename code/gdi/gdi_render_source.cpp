#include "gdi_render_source.h"
#include "gdi_context.h"

namespace bxGdi
{
    bxGdiRenderSource* renderSource_new( int numStreams, bxAllocator* allocator )
    {
        int memSize = 0;
        memSize += sizeof( bxGdiRenderSource );
        memSize += ( numStreams - 1 ) * sizeof( bxGdiVertexBuffer );

        void* mem = BX_MALLOC( allocator, memSize, 8 );

        bxGdiRenderSource* rsource = new(mem)bxGdiRenderSource();
        rsource->numVertexBuffers = numStreams;
        
        for( int istream = 1; istream < numStreams; ++istream )
        {
            new( &rsource->vertexBuffers[istream] ) bxGdiVertexBuffer();
        }
        
        return rsource;
    }
    void renderSource_free( bxGdiRenderSource** rsource, bxAllocator* allocator )
    {
        if( !rsource[0] )
            return;

#ifdef _DEBUG
        SYS_ASSERT( rsource[0]->indexBuffer.id == 0 );
        for( int istream = 0; istream < rsource[0]->numVertexBuffers; ++istream )
        {
            SYS_ASSERT( rsource[0]->vertexBuffers[istream].id == 0 );
        }
#endif
        BX_FREE0( allocator, rsource[0] );
    }

    void renderSource_release( bxGdiDeviceBackend* dev, bxGdiRenderSource* rsource )
    {
        dev->releaseIndexBuffer( &rsource->indexBuffer );
        for( int ibuffer = 0; ibuffer < rsource->numVertexBuffers; ++ibuffer )
        {
            dev->releaseVertexBuffer( &rsource->vertexBuffers[ibuffer] );
        }
    }

    void renderSource_setVertexBuffer( bxGdiRenderSource* rsource, bxGdiVertexBuffer vBuffer, int idx )
    {
        SYS_ASSERT( idx <= rsource->numVertexBuffers );
        SYS_ASSERT( rsource->vertexBuffers[idx].id == 0 );
        
        rsource->vertexBuffers[idx] = vBuffer;
    }
    void renderSource_setIndexBuffer( bxGdiRenderSource* rsource, bxGdiIndexBuffer iBuffer )
    {
        SYS_ASSERT( rsource->indexBuffer.id == 0 );
        rsource->indexBuffer = iBuffer;
    }

    
    void renderSource_enable( bxGdiContext* ctx, bxGdiRenderSource* rsource )
    {
        ctx->setVertexBuffers( rsource->vertexBuffers, rsource->numVertexBuffers );
        ctx->setIndexBuffer( rsource->indexBuffer );
    }

    void renderSurface_draw( bxGdiContext* ctx, const bxGdiRenderSurface& surf )
    {
        ctx->setTopology( surf.topology );
        ctx->draw( surf.count, surf.begin );
    }

    void renderSurface_drawIndexed( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int baseVertex )
    {
        ctx->setTopology( surf.topology );
        ctx->drawIndexed( surf.count, surf.begin, baseVertex );
    }

    void renderSurface_drawInstanced( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int numInstances )
    {
        ctx->setTopology( surf.topology );
        ctx->drawInstanced( surf.count, surf.begin, numInstances );
    }

    void renderSurface_drawIndexedInstanced( bxGdiContext* ctx, const bxGdiRenderSurface& surf, int numInstances, int baseVertex )
    {
        ctx->setTopology( surf.topology );
        ctx->drawIndexedInstanced( surf.count, surf.begin, numInstances, baseVertex );
    }

}///
