#include "../gdi_backend.h"

#include "gdi_backend_dx11.h"

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
        ID3D11Buffer* buffers[bxGdi::eSLOT_COUNT];
        unsigned strides[bxGdi::eSLOT_COUNT];
        unsigned offsets[bxGdi::eSLOT_COUNT];
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

    virtual void changeRenderTargets( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex ) {}
    virtual void clearBuffers( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth ) {}

    virtual void draw( unsigned numVertices, unsigned startIndex ) {}
    virtual void drawIndexed( unsigned numIndices , unsigned startIndex, unsigned baseVertex ) {}
    virtual void drawInstanced( unsigned numVertices, unsigned startIndex, unsigned numInstances ) {}
    virtual void drawIndexedInstanced( unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex ) {}

    virtual unsigned char* mapVertices( bxGdiVertexBuffer vbuffer, int offsetInBytes, int mapType ) {}
    virtual unsigned char* mapIndices( bxGdiIndexBuffer ibuffer, int offsetInBytes, int mapType ) {}
    virtual void unmapVertices( bxGdiVertexBuffer vbuffer ) {}
    virtual void unmapIndices( bxGdiIndexBuffer ibuffer ) {}
    virtual void updateCBuffer( bxGdiBuffer cbuffer, const void* data ) {}
    virtual void swap() {}
    virtual void generateMipmaps( bxGdiTexture texture ) {}

    IDXGISwapChain*		 _swapChain;
    ID3D11DeviceContext* _context;

    ID3D11RenderTargetView* _mainFramebuffer;
    i32 _mainFramebufferWidth;
    i32 _mainFramebufferHeight;
};