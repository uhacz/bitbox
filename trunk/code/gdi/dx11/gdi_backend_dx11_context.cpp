#include "../gdi_backend.h"
#include <util/debug.h>
#include <util/memory.h>
#include "gdi_backend_dx11.h"

namespace bxGdi
{
    unsigned _FetchRenderTargetTextures( ID3D11RenderTargetView** rtv, ID3D11DepthStencilView** dsv, bxGdiTexture* colorTex, unsigned nColorTex, bxGdiTexture depthTex, unsigned slotCount )
    {
        *dsv = depthTex.dx.viewDS;

        for( unsigned i = 0; i < nColorTex; ++i )
        {
            rtv[i] = colorTex[i].dx.viewRT;
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
}///


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
        ID3D11Buffer* buffers[bxGdi::cMAX_VERTEX_BUFFERS];
        unsigned strides[bxGdi::cMAX_VERTEX_BUFFERS];
        unsigned offsets[bxGdi::cMAX_VERTEX_BUFFERS];
        memset( buffers, 0, sizeof(buffers) );
        memset( strides, 0, sizeof(strides) );
        memset( offsets, 0, sizeof(offsets) );

        for( unsigned i = 0; i < n; ++i )
        {
            if( !vbuffers[i].dx11Buffer )
                continue;

            const bxGdiVertexBuffer& buffer = vbuffers[i];
            strides[i] = bxGdi::streamStride( buffer.desc );
            buffers[i] = buffer.dx11Buffer;
        }
        _context->IASetVertexBuffers( start, n, buffers, strides, offsets );
    }
    virtual void setIndexBuffer( bxGdiIndexBuffer ibuffer ) 
    {
        DXGI_FORMAT format = ( ibuffer.dx11Buffer ) ? bxGdi::to_DXGI_FORMAT( ibuffer.dataType, 1 ) : DXGI_FORMAT_UNKNOWN;
        _context->IASetIndexBuffer( ibuffer.dx11Buffer, format, 0 );
    }
    virtual void setShaderPrograms( bxGdiShader* shaders, int n ) 
    {
        for( int i = 0; i < n; ++i )
        {
            bxGdiShader& shader = shaders[i];

            switch( shader.stage )
            {
            case bxGdi::eSTAGE_VERTEX:
                {
                    _context->VSSetShader( shader.dx.vertex, 0, 0 );
                    break;
                }
            case bxGdi::eSTAGE_PIXEL:
                {
                    _context->PSSetShader( shader.dx.pixel, 0, 0 );
                    break;
                }
            case bxGdi::eSTAGE_COMPUTE:
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
        const int SLOT_COUNT = bxGdi::cMAX_CBUFFERS;
        SYS_ASSERT( n <= SLOT_COUNT);
        ID3D11Buffer* buffers[SLOT_COUNT];
        memset( buffers, 0, sizeof(buffers) );

        for( unsigned i = 0; i < n; ++i )
        {
            buffers[i] = cbuffers[i].dx11Buffer;
        }

        switch( stage )
        {
        case bxGdi::eSTAGE_VERTEX:
            {
                _context->VSSetConstantBuffers( startSlot, n, buffers );
                break;
            }
        case bxGdi::eSTAGE_PIXEL:
            {
                _context->PSSetConstantBuffers( startSlot, n, buffers );
                break;
            }
        case bxGdi::eSTAGE_COMPUTE:
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
    virtual void setTextures( bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage ) 
    {
        const int SLOT_COUNT = bxGdi::cMAX_TEXTURES;
        SYS_ASSERT( n <= SLOT_COUNT );

        ID3D11ShaderResourceView* views[SLOT_COUNT];
        memset( views, 0, sizeof(views) );

        if( textures )
        {
            for( unsigned i = 0; i < n; ++i )
            {
                views[i] = textures[i].dx.viewSH;
            }
        }

        switch( stage )
        {
        case bxGdi::eSTAGE_VERTEX:
            {
                _context->VSSetShaderResources( startSlot, n, views );
                break;
            }
        case bxGdi::eSTAGE_PIXEL:
            {
                _context->PSSetShaderResources( startSlot, n, views );
                break;
            }
        case bxGdi::eSTAGE_COMPUTE:
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
        const int SLOT_COUNT = bxGdi::cMAX_SAMPLERS;
        SYS_ASSERT( n <= SLOT_COUNT );
        ID3D11SamplerState* resources[SLOT_COUNT];

        for( unsigned i = 0; i < n; ++i )
        {
            resources[i] = samplers[i].dx.state;
        }

        switch( stage )
        {
        case bxGdi::eSTAGE_VERTEX:
            {
                _context->VSSetSamplers( startSlot, n, resources );
                break;
            }
        case bxGdi::eSTAGE_PIXEL:
            {
                _context->PSSetSamplers( startSlot, n, resources );
                break;
            }
        case bxGdi::eSTAGE_COMPUTE:
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

    virtual void setTopology( int topology )
    {
        _context->IASetPrimitiveTopology( bxGdi::topologyMap[ topology ] );
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
        const int SLOT_COUNT = bxGdi::cMAX_RENDER_TARGETS;
	    SYS_ASSERT(nColor < SLOT_COUNT);

	    ID3D11RenderTargetView *rtv[SLOT_COUNT];
	    ID3D11DepthStencilView *dsv = 0;
        bxGdi::_FetchRenderTargetTextures( rtv, &dsv, colorTex, nColor, depthTex, SLOT_COUNT );

	    _context->OMSetRenderTargets(SLOT_COUNT, rtv, dsv);
    }
    virtual void clearBuffers( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth ) 
    {
        const int SLOT_COUNT = bxGdi::cMAX_RENDER_TARGETS;
	    SYS_ASSERT(nColor < SLOT_COUNT);

	    ID3D11RenderTargetView *rtv[SLOT_COUNT];
	    ID3D11DepthStencilView *dsv = 0;
        bxGdi::_FetchRenderTargetTextures( rtv, &dsv, colorTex, nColor, depthTex, SLOT_COUNT );

        if( ( ( !colorTex || !nColor ) && !depthTex.dx.resource ) && flag_color )
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

    virtual unsigned char* mapVertices( bxGdiVertexBuffer vbuffer, int firstElement, int numElements, int mapType ) 
    {
        SYS_ASSERT( (u32)( firstElement + numElements ) <= vbuffer.numElements );
        
        const int offsetInBytes = firstElement * bxGdi::streamStride( vbuffer.desc );
        const D3D11_MAP dxMapType = ( mapType == bxGdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
        
        return bxGdi::_MapResource( _context, vbuffer.dx11Buffer, dxMapType, offsetInBytes );
    }
    virtual unsigned char* mapIndices( bxGdiIndexBuffer ibuffer, int firstElement, int numElements, int mapType ) 
    {
        SYS_ASSERT( (u32)( firstElement + numElements ) <= ibuffer.numElements );
        SYS_ASSERT( ibuffer.dataType == bxGdi::eTYPE_USHORT || ibuffer.dataType == bxGdi::eTYPE_UINT );

        const int offsetInBytes = firstElement * bxGdi::typeStride[ ibuffer.dataType ];
        const D3D11_MAP dxMapType = ( mapType == bxGdi::eMAP_WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
        
        return bxGdi::_MapResource( _context, ibuffer.dx11Buffer, dxMapType, offsetInBytes );
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
    virtual void swap() 
    {
        _swapChain->Present( 0, 0 );
    }
    virtual void generateMipmaps( bxGdiTexture texture ) 
    {
        _context->GenerateMips( texture.dx.viewSH );
    }

    virtual bxGdiTexture backBufferTexture()
    {
        bxGdiTexture tex;
        tex.id = 0;
        tex.dx.viewRT = _mainFramebuffer;
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


namespace bxGdi
{
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
}//