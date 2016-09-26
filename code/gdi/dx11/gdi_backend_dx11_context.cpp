#include "../gdi_backend.h"
#include <util/debug.h>
#include <util/memory.h>
#include "gdi_backend_dx11.h"

namespace bx{
namespace gdi{

    unsigned _FetchRenderTargetTextures( ID3D11RenderTargetView** rtv, ID3D11DepthStencilView** dsv, bxGdiTexture* colorTex, unsigned nColorTex, bxGdiTexture depthTex, unsigned slotCount )
    {
        *dsv = depthTex.dx11ViewDS;

        for( unsigned i = 0; i < nColorTex; ++i )
        {
            rtv[i] = colorTex[i].dx11ViewRT;
        }
        
        for( unsigned i = nColorTex; i < slotCount; ++i )
        {
            rtv[i] = 0;
        }
        
        return nColorTex; 
    }

    unsigned char* _MapResource( ID3D11DeviceContext* ctx, ID3D11Resource* resource, D3D11_MAP dxMapType, int offsetInBytes )
    {
        D3D11_MAPPED_SUBRESOURCE mappedRes;
        mappedRes.pData = NULL;
        mappedRes.RowPitch = 0;
        mappedRes.DepthPitch = 0;

        ctx->Map( resource, 0, dxMapType, 0, &mappedRes );
        return (u8*)mappedRes.pData + offsetInBytes;
    }
}}///


struct bxGdiContextBackend_dx11 : public bxGdiContextBackend
{
    virtual void clearState()
    {
        _context->ClearState();
    }
    virtual void setViewport( const bxGdiViewport& vp ) 
    {
        D3D11_VIEWPORT dxVp;

        dxVp.Width = (FLOAT)vp.w;
        dxVp.Height = (FLOAT)vp.h;
        dxVp.MinDepth = 0.0f;
        dxVp.MaxDepth = 1.0f;
        dxVp.TopLeftX = (FLOAT)vp.x;
        dxVp.TopLeftY = (FLOAT)vp.y;

        _context->RSSetViewports( 1, &dxVp );
    }
    virtual void setVertexBuffers( bxGdiVertexBuffer* vbuffers, unsigned start, unsigned n )
    {
        ID3D11Buffer* buffers[bx::gdi::cMAX_VERTEX_BUFFERS];
        unsigned strides[bx::gdi::cMAX_VERTEX_BUFFERS];
        unsigned offsets[bx::gdi::cMAX_VERTEX_BUFFERS];
        memset( buffers, 0, sizeof(buffers) );
        memset( strides, 0, sizeof(strides) );
        memset( offsets, 0, sizeof(offsets) );

        for( unsigned i = 0; i < n; ++i )
        {
            if( !vbuffers[i].dx11Buffer )
                continue;

            const bxGdiVertexBuffer& buffer = vbuffers[i];
            strides[i] = bx::gdi::streamStride( buffer.desc );
            buffers[i] = buffer.dx11Buffer;
        }
        _context->IASetVertexBuffers( start, n, buffers, strides, offsets );
    }
    virtual void setIndexBuffer( bxGdiIndexBuffer ibuffer ) 
    {
        DXGI_FORMAT format = ( ibuffer.dx11Buffer ) ? bx::gdi::to_DXGI_FORMAT( ibuffer.dataType, 1 ) : DXGI_FORMAT_UNKNOWN;
        _context->IASetIndexBuffer( ibuffer.dx11Buffer, format, 0 );
    }
    virtual void setShaderPrograms( bxGdiShader* shaders, int n ) 
    {
        for( int i = 0; i < n; ++i )
        {
            bxGdiShader& shader = shaders[i];
            if( !shader.id )
                continue;

            switch( shader.stage )
            {
            case bx::gdi::eSTAGE_VERTEX:
                {
                    _context->VSSetShader( shader.dx.vertex, 0, 0 );
                    break;
                }
            case bx::gdi::eSTAGE_PIXEL:
                {
                    _context->PSSetShader( shader.dx.pixel, 0, 0 );
                    break;
                }
            case bx::gdi::eSTAGE_COMPUTE:
                {
                    _context->CSSetShader( shader.dx.compute, 0, 0 );
                    break;
                }
            default:
                {
                    SYS_ASSERT( false );
                    break;
                }
            }
        }
    }
    virtual void setInputLayout( const bxGdiInputLayout ilay )
    {
        _context->IASetInputLayout( ilay.dx.lay );
    }

    virtual void setCbuffers( bxGdiBuffer* cbuffers , unsigned startSlot, unsigned n, int stage )
    {
        const int SLOT_COUNT = bx::gdi::cMAX_CBUFFERS;
        SYS_ASSERT( n <= SLOT_COUNT);
        ID3D11Buffer* buffers[SLOT_COUNT];
        memset( buffers, 0, sizeof(buffers) );

        for( unsigned i = 0; i < n; ++i )
        {
            buffers[i] = cbuffers[i].dx11Buffer;
        }

        switch( stage )
        {
        case bx::gdi::eSTAGE_VERTEX:
            {
                _context->VSSetConstantBuffers( startSlot, n, buffers );
                break;
            }
        case bx::gdi::eSTAGE_PIXEL:
            {
                _context->PSSetConstantBuffers( startSlot, n, buffers );
                break;
            }
        case bx::gdi::eSTAGE_COMPUTE:
            {
                _context->CSSetConstantBuffers( startSlot, n, buffers );
                break;
            }
        default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }
    
    virtual void setResourcesRO( bxGdiResource* resources, unsigned startSlot, unsigned n, int stage )
    {
        const int SLOT_COUNT = bx::gdi::cMAX_RESOURCES_RO;
        SYS_ASSERT( n <= SLOT_COUNT );

        ID3D11ShaderResourceView* views[SLOT_COUNT];
        memset( views, 0, sizeof( views ) );

        if ( resources )
        {
            for ( unsigned i = 0; i < n; ++i )
            {
                views[i] = resources[i].dx11ViewSH;
            }
        }

        switch ( stage )
        {
            case bx::gdi::eSTAGE_VERTEX:
            {
                _context->VSSetShaderResources( startSlot, n, views );
                break;
            }
            case bx::gdi::eSTAGE_PIXEL:
            {
                _context->PSSetShaderResources( startSlot, n, views );
                break;
            }
            case bx::gdi::eSTAGE_COMPUTE:
            {
                _context->CSSetShaderResources( startSlot, n, views );
                break;
            }
            default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }

    virtual void setTextures( bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage ) 
    {
        const int SLOT_COUNT = bx::gdi::cMAX_TEXTURES;
        SYS_ASSERT( n <= SLOT_COUNT );

        ID3D11ShaderResourceView* views[SLOT_COUNT];
        memset( views, 0, sizeof(views) );

        if( textures )
        {
            for( unsigned i = 0; i < n; ++i )
            {
                views[i] = textures[i].rs.dx11ViewSH;
            }
        }

        switch( stage )
        {
        case bx::gdi::eSTAGE_VERTEX:
            {
                _context->VSSetShaderResources( startSlot, n, views );
                break;
            }
        case bx::gdi::eSTAGE_PIXEL:
            {
                _context->PSSetShaderResources( startSlot, n, views );
                break;
            }
        case bx::gdi::eSTAGE_COMPUTE:
            {
                _context->CSSetShaderResources( startSlot, n, views );
                break;
            }
        default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }
    virtual void setSamplers( bxGdiSampler* samplers, unsigned startSlot, unsigned n, int stage )
    {
        const int SLOT_COUNT = bx::gdi::cMAX_SAMPLERS;
        SYS_ASSERT( n <= SLOT_COUNT );
        ID3D11SamplerState* resources[SLOT_COUNT];

        for( unsigned i = 0; i < n; ++i )
        {
            resources[i] = samplers[i].dx.state;
        }

        switch( stage )
        {
        case bx::gdi::eSTAGE_VERTEX:
            {
                _context->VSSetSamplers( startSlot, n, resources );
                break;
            }
        case bx::gdi::eSTAGE_PIXEL:
            {
                _context->PSSetSamplers( startSlot, n, resources );
                break;
            }
        case bx::gdi::eSTAGE_COMPUTE:
            {
                _context->CSSetSamplers( startSlot, n, resources );
                break;
            }
        default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }

    virtual void setDepthState( const bxGdiDepthState state )
    {
        _context->OMSetDepthStencilState( state.dx.state, 0 );
    }
    virtual void setBlendState( const bxGdiBlendState state )
    {
        const float bfactor[4] = {0.f, 0.f, 0.f, 0.f};
        _context->OMSetBlendState( state.dx.state, bfactor, 0xFFFFFFFF );
    }
    virtual void setRasterState( const bxGdiRasterState state )
    {
        _context->RSSetState( state.dx.state );
    }
    virtual void setScissorRects( const bxGdiRect* rects, int n )
    {
        const int cMAX_RECTS = 4;
        D3D11_RECT dx11Rects[cMAX_RECTS];
        SYS_ASSERT( n <= cMAX_RECTS );

        for ( int i = 0; i < cMAX_RECTS; ++i )
        {
            D3D11_RECT& r = dx11Rects[i];
            r.left = rects[i].left;
            r.top = rects[i].top;
            r.right = rects[i].right;
            r.bottom = rects[i].bottom;
        }

        _context->RSSetScissorRects( n, dx11Rects );
    }

    virtual void setTopology( int topology )
    {
        _context->IASetPrimitiveTopology( bx::gdi::topologyMap[ topology ] );
    }

    virtual void changeToMainFramebuffer()
    {
        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        vp.Width = (float)_mainFramebufferWidth;
        vp.Height = (float)_mainFramebufferHeight;

        _context->OMSetRenderTargets(1, &_mainFramebuffer, 0);
        _context->RSSetViewports(1, &vp);
    }
    
    virtual void changeRenderTargets( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex ) 
    {
        const int SLOT_COUNT = bx::gdi::cMAX_RENDER_TARGETS;
	    SYS_ASSERT(nColor < SLOT_COUNT);

	    ID3D11RenderTargetView *rtv[SLOT_COUNT];
	    ID3D11DepthStencilView *dsv = 0;
        bx::gdi::_FetchRenderTargetTextures( rtv, &dsv, colorTex, nColor, depthTex, SLOT_COUNT );

	    _context->OMSetRenderTargets(SLOT_COUNT, rtv, dsv);
    }
    virtual void clearBuffers( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth ) 
    {
        const int SLOT_COUNT = bx::gdi::cMAX_RENDER_TARGETS;
	    SYS_ASSERT(nColor < SLOT_COUNT);

	    ID3D11RenderTargetView *rtv[SLOT_COUNT];
	    ID3D11DepthStencilView *dsv = 0;
        bx::gdi::_FetchRenderTargetTextures( rtv, &dsv, colorTex, nColor, depthTex, SLOT_COUNT );

        if( ( ( !colorTex || !nColor ) && !depthTex.rs.dx11Resource ) && flag_color )
        {
            _context->ClearRenderTargetView( _mainFramebuffer, rgbad );
        }
        else
        {
            if( flag_depth && dsv )
            {
                _context->ClearDepthStencilView( dsv, D3D11_CLEAR_DEPTH, rgbad[4], 0 );
            }

            if( flag_color && nColor )
            {
                for( unsigned i = 0; i < nColor; ++i )
                {
                    _context->ClearRenderTargetView( rtv[i], rgbad );
                }
            }
        }
    }

    virtual void draw( unsigned numVertices, unsigned startIndex ) 
    {
        _context->Draw( numVertices, startIndex );
    }
    virtual void drawIndexed( unsigned numIndices , unsigned startIndex, unsigned baseVertex ) 
    {
        _context->DrawIndexed( numIndices, startIndex, baseVertex );
    }
    virtual void drawInstanced( unsigned numVertices, unsigned startIndex, unsigned numInstances ) 
    {
        _context->DrawInstanced( numVertices, numInstances, startIndex, 0 );
    }
    virtual void drawIndexedInstanced( unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex ) 
    {
        _context->DrawIndexedInstanced( numIndices, numInstances, startIndex, baseVertex, 0 );
    }

    virtual unsigned char* map( bxGdiResource resource, int offsetInBytes, int mapType = bx::gdi::eMAP_WRITE )
    {
        const D3D11_MAP dxMapType = (mapType == bx::gdi::eMAP_WRITE) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
        return bx::gdi::_MapResource( _context, resource.dx11Resource, dxMapType, offsetInBytes );
    }
    virtual void unmap( bxGdiResource resource )
    {
        _context->Unmap( resource.dx11Resource, 0 );
    }

    virtual unsigned char* mapVertices( bxGdiVertexBuffer vbuffer, int firstElement, int numElements, int mapType ) 
    {
        SYS_ASSERT( (u32)( firstElement + numElements ) <= vbuffer.numElements );
        
        const int offsetInBytes = firstElement * bx::gdi::streamStride( vbuffer.desc );
        const D3D11_MAP dxMapType = ( mapType == bx::gdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
        
        return bx::gdi::_MapResource( _context, vbuffer.dx11Buffer, dxMapType, offsetInBytes );
    }
    virtual unsigned char* mapIndices( bxGdiIndexBuffer ibuffer, int firstElement, int numElements, int mapType ) 
    {
        SYS_ASSERT( (u32)( firstElement + numElements ) <= ibuffer.numElements );
        SYS_ASSERT( ibuffer.dataType == bx::gdi::eTYPE_USHORT || ibuffer.dataType == bx::gdi::eTYPE_UINT );

        const int offsetInBytes = firstElement * bx::gdi::typeStride[ ibuffer.dataType ];
        const D3D11_MAP dxMapType = ( mapType == bx::gdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
        
        return bx::gdi::_MapResource( _context, ibuffer.dx11Buffer, dxMapType, offsetInBytes );
    }
    virtual void unmapVertices( bxGdiVertexBuffer vbuffer ) 
    {
        _context->Unmap( vbuffer.dx11Buffer, 0 );
    }
    virtual void unmapIndices( bxGdiIndexBuffer ibuffer ) 
    {
        _context->Unmap( ibuffer.dx11Buffer, 0 );
    }
    virtual void updateCBuffer( bxGdiBuffer cbuffer, const void* data ) 
    {
        _context->UpdateSubresource( cbuffer.dx11Buffer, 0, NULL, data, 0, 0 );
    }
    virtual void updateTexture( bxGdiTexture texture, const void* data )
    {
        const u32 formatWidth = bx::gdi::formatByteWidth( texture.format );
        const u32 srcRowPitch = formatWidth * texture.width;
        const u32 srcDepthPitch = srcRowPitch * texture.height;
        _context->UpdateSubresource( texture.rs.dx11Resource, 0, NULL, data, srcRowPitch, srcDepthPitch );
    }
    virtual void swap() 
    {
        _swapChain->Present( 1, 0 );
    }
    virtual void generateMipmaps( bxGdiTexture texture ) 
    {
        _context->GenerateMips( texture.rs.dx11ViewSH );
    }

    virtual bxGdiTexture backBufferTexture()
    {
        bxGdiTexture tex;
        tex.id = 0;
        tex.dx11ViewRT = _mainFramebuffer;
        tex.width = _mainFramebufferWidth;
        tex.height = _mainFramebufferHeight;

        return tex;
    }

    bxGdiContextBackend_dx11()
        : _swapChain(0)
        , _context(0)
        , _mainFramebuffer(0)
        , _mainFramebufferWidth(0)
        , _mainFramebufferHeight(0)
    {}

    virtual ~bxGdiContextBackend_dx11()
    {
        _mainFramebuffer->Release();
        _context->Release();
        _swapChain->Release();
    }

    IDXGISwapChain*		 _swapChain;
    ID3D11DeviceContext* _context;

    ID3D11RenderTargetView* _mainFramebuffer;
    i32 _mainFramebufferWidth;
    i32 _mainFramebufferHeight;
};


namespace bx{
namespace gdi{

    bxGdiContextBackend* newContext_dx11( ID3D11DeviceContext* contextDx11, IDXGISwapChain* swapChain )
    {
        bxGdiContextBackend_dx11* ctx = BX_NEW( bxDefaultAllocator(), bxGdiContextBackend_dx11 );
        ctx->_context = contextDx11;
        ctx->_swapChain = swapChain;

        ID3D11Device* deviceDx11 = 0;
        contextDx11->GetDevice( &deviceDx11 );

        ID3D11Texture2D* backBuffer = NULL;
	    HRESULT hres = swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&backBuffer );
	    SYS_ASSERT( SUCCEEDED(hres) );

	    ID3D11RenderTargetView* viewRT = 0;
	    hres = deviceDx11->CreateRenderTargetView( backBuffer, NULL, &viewRT );
	    SYS_ASSERT( SUCCEEDED(hres) );
	    ctx->_mainFramebuffer = viewRT;

	    D3D11_TEXTURE2D_DESC bbdesc;
	    backBuffer->GetDesc( &bbdesc );
        ctx->_mainFramebufferWidth  = bbdesc.Width;
        ctx->_mainFramebufferHeight = bbdesc.Height;
        
        backBuffer->Release();
        deviceDx11->Release();

        return ctx;
    }
}}//


namespace bx{
namespace gdi{

//////////////////////////////////////////////////////////////////////////
namespace set
{
void viewport( CommandQueue* cmdq, Viewport vp )
{
    D3D11_VIEWPORT dxVp;

    dxVp.Width = (FLOAT)vp.w;
    dxVp.Height = (FLOAT)vp.h;
    dxVp.MinDepth = 0.0f;
    dxVp.MaxDepth = 1.0f;
    dxVp.TopLeftX = (FLOAT)vp.x;
    dxVp.TopLeftY = (FLOAT)vp.y;

    cmdq->dx11()->RSSetViewports( 1, &dxVp );
}
void vertexBuffers( CommandQueue* cmdq, VertexBuffer* vbuffers, unsigned start, unsigned n )
{
    ID3D11Buffer* buffers[bx::gdi::cMAX_VERTEX_BUFFERS];
    unsigned strides[bx::gdi::cMAX_VERTEX_BUFFERS];
    unsigned offsets[bx::gdi::cMAX_VERTEX_BUFFERS];
    memset( buffers, 0, sizeof( buffers ) );
    memset( strides, 0, sizeof( strides ) );
    memset( offsets, 0, sizeof( offsets ) );

    for( unsigned i = 0; i < n; ++i )
    {
        if( !vbuffers[i].buffer )
            continue;

        const VertexBuffer& buffer = vbuffers[i];
        strides[i] = bx::gdi::blockStride( buffer.desc );
        buffers[i] = buffer.buffer;
    }
    cmdq->dx11()->IASetVertexBuffers( start, n, buffers, strides, offsets );
}
void indexBuffer( CommandQueue* cmdq, IndexBuffer ibuffer )
{
    DXGI_FORMAT format = ( ibuffer.buffer ) ? bx::gdi::to_DXGI_FORMAT( ibuffer.dataType, 1 ) : DXGI_FORMAT_UNKNOWN;
    cmdq->dx11()->IASetIndexBuffer( ibuffer.buffer, format, 0 );
}
void shaderPrograms( CommandQueue* cmdq, Shader* shaders, int n )
{
    for( int i = 0; i < n; ++i )
    {
        Shader& shader = shaders[i];
        if( !shader.id )
            continue;

        switch( shader.stage )
        {
        case bx::gdi::eSTAGE_VERTEX:
            {
                cmdq->dx11()->VSSetShader( shader.vertex, 0, 0 );
                break;
            }
        case bx::gdi::eSTAGE_PIXEL:
            {
                cmdq->dx11()->PSSetShader( shader.pixel, 0, 0 );
                break;
            }
        case bx::gdi::eSTAGE_COMPUTE:
            {
                cmdq->dx11()->CSSetShader( shader.compute, 0, 0 );
                break;
            }
        default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }
}
void inputLayout( CommandQueue* cmdq, InputLayout ilay )
{
    cmdq->dx11()->IASetInputLayout( ilay.layout );
}

void cbuffers( CommandQueue* cmdq, ConstantBuffer* cbuffers, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = bx::gdi::cMAX_CBUFFERS;
    SYS_ASSERT( n <= SLOT_COUNT );
    ID3D11Buffer* buffers[SLOT_COUNT];
    memset( buffers, 0, sizeof( buffers ) );

    for( unsigned i = 0; i < n; ++i )
    {
        buffers[i] = cbuffers[i].buffer;
    }

    if( stageMask & bx::gdi::eSTAGE_MASK_VERTEX )
        cmdq->dx11()->VSSetConstantBuffers( startSlot, n, buffers );
        
    if( stageMask & bx::gdi::eSTAGE_MASK_PIXEL )
        cmdq->dx11()->PSSetConstantBuffers( startSlot, n, buffers );
        
    if( stageMask & bx::gdi::eSTAGE_MASK_COMPUTE )
        cmdq->dx11()->CSSetConstantBuffers( startSlot, n, buffers );
}
void resourcesRO( CommandQueue* cmdq, ResourceRO* resources, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = bx::gdi::cMAX_RESOURCES_RO;
    SYS_ASSERT( n <= SLOT_COUNT );

    ID3D11ShaderResourceView* views[SLOT_COUNT];
    memset( views, 0, sizeof( views ) );

    if( resources )
    {
        for( unsigned i = 0; i < n; ++i )
        {
            views[i] = resources[i].viewSH;
        }
    }

    if( stageMask & bx::gdi::eSTAGE_MASK_VERTEX )
        cmdq->dx11()->VSSetShaderResources( startSlot, n, views );

    if( stageMask & bx::gdi::eSTAGE_MASK_PIXEL )
        cmdq->dx11()->PSSetShaderResources( startSlot, n, views );

    if( stageMask & bx::gdi::eSTAGE_MASK_COMPUTE )
        cmdq->dx11()->CSSetShaderResources( startSlot, n, views );
}
void resourcesRW( CommandQueue* cmdq, ResourceRW* resources, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = bx::gdi::cMAX_RESOURCES_RW;
    SYS_ASSERT( n <= SLOT_COUNT );

    ID3D11UnorderedAccessView* views[SLOT_COUNT];
    UINT initial_count[SLOT_COUNT] = {};

    memset( views, 0, sizeof( views ) );

    if( resources )
    {
        for( unsigned i = 0; i < n; ++i )
        {
            views[i] = resources[i].viewUA;
        }
    }

    if( stageMask & gdi::eSTAGE_MASK_VERTEX || stageMask & gdi::eSTAGE_MASK_PIXEL )
    {
        bxLogError( "ResourceRW can be set only in compute stage" );
    }

    if( stageMask & gdi::eSTAGE_COMPUTE )
        cmdq->dx11()->CSSetUnorderedAccessViews( startSlot, n, views, initial_count );
}

void samplers( CommandQueue* cmdq, Sampler* samplers, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = bx::gdi::cMAX_SAMPLERS;
    SYS_ASSERT( n <= SLOT_COUNT );
    ID3D11SamplerState* resources[SLOT_COUNT];

    for( unsigned i = 0; i < n; ++i )
    {
        resources[i] = samplers[i].state;
    }

    if( stageMask & bx::gdi::eSTAGE_MASK_VERTEX )
        cmdq->dx11()->VSSetSamplers( startSlot, n, resources );
     
    if( stageMask & bx::gdi::eSTAGE_MASK_PIXEL )
        cmdq->dx11()->PSSetSamplers( startSlot, n, resources );
        
    if( stageMask & bx::gdi::eSTAGE_MASK_COMPUTE )
        cmdq->dx11()->CSSetSamplers( startSlot, n, resources );
}

void depthState( CommandQueue* cmdq, DepthState state )
{
    cmdq->dx11()->OMSetDepthStencilState( state.state, 0 );
}
void blendState( CommandQueue* cmdq, BlendState state )
{
    const float bfactor[4] = { 0.f, 0.f, 0.f, 0.f };
    cmdq->dx11()->OMSetBlendState( state.state, bfactor, 0xFFFFFFFF );
}
void rasterState( CommandQueue* cmdq, RasterState state )
{
    cmdq->dx11()->RSSetState( state.state );
}
void scissorRects( CommandQueue* cmdq, const Rect* rects, int n )
{
    const int cMAX_RECTS = 4;
    D3D11_RECT dx11Rects[cMAX_RECTS];
    SYS_ASSERT( n <= cMAX_RECTS );

    for( int i = 0; i < cMAX_RECTS; ++i )
    {
        D3D11_RECT& r = dx11Rects[i];
        r.left = rects[i].left;
        r.top = rects[i].top;
        r.right = rects[i].right;
        r.bottom = rects[i].bottom;
    }

    cmdq->dx11()->RSSetScissorRects( n, dx11Rects );
}
void topology( CommandQueue* cmdq, int topology )
{
    cmdq->dx11()->IASetPrimitiveTopology( bx::gdi::topologyMap[topology] );
}

void changeToMainFramebuffer( CommandQueue* cmdq )
{
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.Width = (float)cmdq->_mainFramebufferWidth;
    vp.Height = (float)cmdq->_mainFramebufferHeight;

    cmdq->dx11()->OMSetRenderTargets( 1, &cmdq->_mainFramebuffer, 0 );
    cmdq->dx11()->RSSetViewports( 1, &vp );
}
void changeRenderTargets( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex )
{
    const int SLOT_COUNT = bx::gdi::cMAX_RENDER_TARGETS;
    SYS_ASSERT( nColor < SLOT_COUNT );

    ID3D11RenderTargetView *rtv[SLOT_COUNT] = {};
    ID3D11DepthStencilView *dsv = depthTex.viewDS;

    for( u32 i = 0; i < nColor; ++i )
    {
        rtv[i] = colorTex[i].viewRT;
    }

    cmdq->dx11()->OMSetRenderTargets( SLOT_COUNT, rtv, dsv );
}
}///

//////////////////////////////////////////////////////////////////////////
namespace data
{
unsigned char* map( CommandQueue* cmdq, Resource resource, int offsetInBytes, int mapType )
{
    const D3D11_MAP dxMapType = ( mapType == bx::gdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
    return bx::gdi::_MapResource( cmdq->dx11(), resource.resource, dxMapType, offsetInBytes );
}
void unmap( CommandQueue* cmdq, Resource resource )
{
    cmdq->dx11()->Unmap( resource.resource, 0 );
}

unsigned char* map( CommandQueue* cmdq, VertexBuffer vbuffer, int firstElement, int numElements, int mapType )
{
    SYS_ASSERT( (u32)( firstElement + numElements ) <= vbuffer.numElements );

    const int offsetInBytes = firstElement * bx::gdi::blockStride( vbuffer.desc );
    const D3D11_MAP dxMapType = ( mapType == bx::gdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;

    return bx::gdi::_MapResource( cmdq->dx11(), vbuffer.resource, dxMapType, offsetInBytes );
}
unsigned char* map( CommandQueue* cmdq, IndexBuffer ibuffer, int firstElement, int numElements, int mapType )
{
    SYS_ASSERT( (u32)( firstElement + numElements ) <= ibuffer.numElements );
    SYS_ASSERT( ibuffer.dataType == bx::gdi::eTYPE_USHORT || ibuffer.dataType == bx::gdi::eTYPE_UINT );

    const int offsetInBytes = firstElement * bx::gdi::typeStride[ibuffer.dataType];
    const D3D11_MAP dxMapType = ( mapType == bx::gdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;

    return bx::gdi::_MapResource( cmdq->dx11(), ibuffer.resource, dxMapType, offsetInBytes );
}

void updateCBuffer( CommandQueue* cmdq, ConstantBuffer cbuffer, const void* data )
{
    cmdq->dx11()->UpdateSubresource( cbuffer.resource, 0, NULL, data, 0, 0 );
}
void updateTexture( CommandQueue* cmdq, TextureRW texture, const void* data )
{
    const u32 formatWidth = bx::gdi::formatByteWidth( texture.format );
    const u32 srcRowPitch = formatWidth * texture.width;
    const u32 srcDepthPitch = srcRowPitch * texture.height;
    cmdq->dx11()->UpdateSubresource( texture.resource, 0, NULL, data, srcRowPitch, srcDepthPitch );
}

}///

//////////////////////////////////////////////////////////////////////////
namespace submit
{
void draw( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex )
{
    cmdq->dx11()->Draw( numVertices, startIndex );
}
void drawIndexed( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned baseVertex )
{
    cmdq->dx11()->DrawIndexed( numIndices, startIndex, baseVertex );
}
void drawInstanced( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex, unsigned numInstances )
{
    cmdq->dx11()->DrawInstanced( numVertices, numInstances, startIndex, 0 );
}
void drawIndexedInstanced( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned numInstances, unsigned baseVertex )
{
    cmdq->dx11()->DrawIndexedInstanced( numIndices, numInstances, startIndex, baseVertex, 0 );
}
}///


//////////////////////////////////////////////////////////////////////////
namespace misc
{
void clearState( CommandQueue* cmdq )
{
    cmdq->dx11()->ClearState();
}
void clearBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, float rgbad[5], int flag_color, int flag_depth )
{
    const int SLOT_COUNT = bx::gdi::cMAX_RENDER_TARGETS;
    SYS_ASSERT( nColor < SLOT_COUNT );

    ID3D11RenderTargetView *rtv[SLOT_COUNT] = {};
    ID3D11DepthStencilView *dsv = 0;
    for( u32 i = 0; i < nColor; ++i )
    {
        rtv[i] = colorTex[i].viewRT;
    }

    if( ( ( !colorTex || !nColor ) && !depthTex.resource ) && flag_color )
    {
        cmdq->dx11()->ClearRenderTargetView( cmdq->_mainFramebuffer, rgbad );
    }
    else
    {
        if( flag_depth && dsv )
        {
            cmdq->dx11()->ClearDepthStencilView( dsv, D3D11_CLEAR_DEPTH, rgbad[4], 0 );
        }

        if( flag_color && nColor )
        {
            for( unsigned i = 0; i < nColor; ++i )
            {
                cmdq->dx11()->ClearRenderTargetView( rtv[i], rgbad );
            }
        }
    }
}

void swap( CommandQueue* cmdq )
{
    cmdq->_swapChain->Present( 1, 0 );
}
void generateMipmaps( CommandQueue* cmdq, TextureRW texture )
{
    cmdq->dx11()->GenerateMips( texture.viewSH );
}
TextureRW backBufferTexture( CommandQueue* cmdq )
{
    TextureRW tex;
    tex.id = 0;
    tex.viewRT = cmdq->_mainFramebuffer;
    tex.width  = cmdq->_mainFramebufferWidth;
    tex.height = cmdq->_mainFramebufferHeight;

    return tex;
}
}///

}}///
