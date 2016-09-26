#pragma once

#include <util/debug.h>
#include "gdi_backend_struct.h"

namespace bx{
namespace gdi{

    int      blockStride( const bxGdiVertexStreamBlock& block ); // { return typeStride[block.dataType] * block.numElements; }
    int      streamStride(const bxGdiVertexStreamDesc& vdesc );
    unsigned streamSlotMask( const bxGdiVertexStreamDesc& vdesc );
    inline int streamEqual( const bxGdiVertexStreamDesc& a, const bxGdiVertexStreamDesc& b )
    {
        SYS_STATIC_ASSERT( bxGdiVertexStreamDesc::eMAX_BLOCKS == 5 );
        return ( a.count == b.count )
            && ( a.blocks[0].hash == b.blocks[0].hash )
            && ( a.blocks[1].hash == b.blocks[1].hash )
            && ( a.blocks[2].hash == b.blocks[2].hash )
            && ( a.blocks[3].hash == b.blocks[3].hash )
            && ( a.blocks[4].hash == b.blocks[4].hash );
    }
}}///

union bxGdiResource
{
    uptr id;
    struct
    {
        ID3D11Resource* dx11Resource;
        ID3D11ShaderResourceView* dx11ViewSH;
        ID3D11UnorderedAccessView* dx11ViewUA;
    };

    static bxGdiResource null() 
    { 
        bxGdiResource rs; 
        rs.id = 0;
        rs.dx11ViewSH = 0;
        rs.dx11ViewUA = 0;
        return rs;
    }

};

struct bxGdiVertexBuffer
{
    union 
    {
        uptr id;
        ID3D11Buffer* dx11Buffer;
    };
    bxGdiVertexStreamDesc desc;
    u32 numElements;

    bxGdiVertexBuffer()
        : id(0)
        , numElements(0)
    {}
};
struct bxGdiIndexBuffer
{
    union 
    {
        uptr id;
        ID3D11Buffer* dx11Buffer;
    };
    u32 dataType;
    u32 numElements;

    bxGdiIndexBuffer()
        : id(0)
        , dataType(0)
        , numElements(0)
    {}
};

struct bxGdiBuffer
{
    union
    {
        uptr id;
        ID3D11Buffer* dx11Buffer;
        bxGdiResource rs;
    };

    u32 sizeInBytes;
    u32 bindFlags;
    bxGdiFormat format;

    bxGdiBuffer()
        : id(0)
        , sizeInBytes(0)
        , bindFlags(0)
    {
        rs = bxGdiResource::null();
    }
};

struct bxGdiTexture
{
    union
    {
        uptr id;
        bxGdiResource rs;
        union
        {
            ID3D11Texture1D* dx11Tex1D;
            ID3D11Texture2D* dx11Tex2D;
            ID3D11Texture3D* dx11Tex3D;
        };
    };
    ID3D11RenderTargetView*     dx11ViewRT;
    ID3D11DepthStencilView*     dx11ViewDS;

    i16 width;
    i16 height;
    i16 depth;
    bxGdiFormat format;

    bxGdiTexture()
        : id( 0 )
        , dx11ViewDS(0)
        , dx11ViewRT(0)
        , width( 0 ), height( 0 ), depth( 0 )
    {
        rs = bxGdiResource::null();
    }
};

namespace bx{
namespace gdi{
    inline bool texture_equal( const bxGdiTexture& a, const bxGdiTexture& b )
    {
        return a.id == b.id;
    }
}}///


struct bxGdiShader
{
    union 
    {
        uptr id;
        struct 
        {
            union 
            {
                ID3D11DeviceChild*      object;
                ID3D11VertexShader*     vertex;
                ID3D11PixelShader*      pixel;
                ID3D11ComputeShader*    compute;
            };
            void* inputSignature;
        } dx;
    };

    i16 stage;
    u16 vertexInputMask;

    bxGdiShader()
        : id(0)
        , stage(-1)
        , vertexInputMask(0)
    {}
};

union bxGdiInputLayout
{
    uptr id;
    struct  
    {
        ID3D11InputLayout* lay;
    } dx;

    bxGdiInputLayout()
        : id(0)
    {}
};
union bxGdiBlendState 
{
    uptr id;
    struct  
    {
        ID3D11BlendState* state;
    } dx;

    bxGdiBlendState()
        : id(0)
    {}
};
union bxGdiDepthState 
{
    uptr id;
    struct  
    {
        ID3D11DepthStencilState* state;
    } dx;

    bxGdiDepthState()
        : id(0)
    {}
};
union bxGdiRasterState
{
    uptr id;
    struct  
    {
        ID3D11RasterizerState* state;
    } dx;

    bxGdiRasterState()
        : id(0)
    {}
};

union bxGdiSampler    
{
    uptr id;
    struct  
    {
        ID3D11SamplerState* state;
    } dx;

    bxGdiSampler()
        : id(0)
    {}
};

struct bxGdiRect
{
    i32 left, top, right, bottom;

    bxGdiRect() {}
    bxGdiRect( int l, int t, int r, int b )
        : left(l), top(t), right(r), bottom(b)
    {}
};

struct bxGdiContextBackend;
struct bxGdiDeviceBackend
{
    virtual ~bxGdiDeviceBackend() {}

    virtual bxGdiVertexBuffer createVertexBuffer( const bxGdiVertexStreamDesc& desc, u32 numElements, const void* data = 0 ) = 0;
    virtual bxGdiIndexBuffer  createIndexBuffer( int dataType, u32 numElements, const void* data = 0  ) = 0;
    virtual bxGdiBuffer createConstantBuffer( u32 sizeInBytes ) = 0;
    virtual bxGdiBuffer createBuffer( int numElements, bxGdiFormat format, unsigned bindFlags, unsigned cpuAccessFlag, unsigned gpuAccessFlag ) = 0;
    
    
    virtual bxGdiShader createShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bx::gdi::ShaderReflection* reflection = 0) = 0;
    virtual bxGdiShader createShader( int stage, const void* codeBlob, size_t codeBlobSizee, bx::gdi::ShaderReflection* reflection = 0  ) = 0;
    
    virtual bxGdiTexture createTexture( const void* dataBlob, size_t dataBlobSize ) = 0;
    virtual bxGdiTexture createTexture1D( int w, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data ) = 0;
    virtual bxGdiTexture createTexture2D( int w, int h, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data ) = 0;
    virtual bxGdiTexture createTexture2Ddepth( int w, int h, int mips, bx::gdi::EDataType dataType, unsigned bindFlags ) = 0;
    virtual bxGdiTexture createTexture3D() = 0;
    virtual bxGdiTexture createTextureCube() = 0;
    virtual bxGdiSampler createSampler( const bxGdiSamplerDesc& desc ) = 0;
    
    virtual bxGdiInputLayout createInputLayout( const bxGdiVertexStreamDesc* descs, int ndescs, bxGdiShader vertex_shader ) = 0;
    virtual bxGdiInputLayout createInputLayout( const bxGdiVertexStreamBlock* blocks, int nblocks, bxGdiShader vertex_shader ) = 0;
    virtual bxGdiBlendState  createBlendState( bxGdiHwStateDesc::Blend blend ) = 0;
    virtual bxGdiDepthState  createDepthState( bxGdiHwStateDesc::Depth depth ) = 0;
    virtual bxGdiRasterState createRasterState( bxGdiHwStateDesc::Raster raster ) = 0;

    virtual void releaseVertexBuffer( bxGdiVertexBuffer* id ) = 0;
    virtual void releaseIndexBuffer( bxGdiIndexBuffer* id ) = 0;
    virtual void releaseInputLayout( bxGdiInputLayout * id ) = 0;
    virtual void releaseBuffer( bxGdiBuffer* id ) = 0;
    virtual void releaseShader( bxGdiShader* id ) = 0;
    virtual void releaseTexture( bxGdiTexture* id ) = 0;
    virtual void releaseSampler( bxGdiSampler* id ) = 0;
    virtual void releaseBlendState ( bxGdiBlendState  * id ) = 0;
    virtual void releaseDepthState ( bxGdiDepthState  * id ) = 0;
    virtual void releaseRasterState( bxGdiRasterState * id ) = 0;


    bxGdiContextBackend* ctx;
};

struct bxGdiContextBackend
{
    virtual ~bxGdiContextBackend() {}
    virtual void clearState             () = 0;
    virtual void setViewport            ( const bxGdiViewport& vp ) = 0;
    virtual void setVertexBuffers       ( bxGdiVertexBuffer* vbuffers, unsigned start, unsigned n ) = 0;
    virtual void setIndexBuffer         ( bxGdiIndexBuffer ibuffer ) = 0;
    virtual void setShaderPrograms      ( bxGdiShader* shaders, int n ) = 0;
    virtual void setInputLayout         ( const bxGdiInputLayout ilay ) = 0;

    virtual void setCbuffers            ( bxGdiBuffer* cbuffers, unsigned startSlot, unsigned n, int stage ) = 0;
    virtual void setResourcesRO         ( bxGdiResource* resources, unsigned startSlot, unsigned n, int stage ) = 0;
    virtual void setTextures            ( bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage ) = 0;
    virtual void setSamplers            ( bxGdiSampler* samplers, unsigned startSlot, unsigned n, int stage ) = 0;

    virtual void setDepthState          ( const bxGdiDepthState state ) = 0;
    virtual void setBlendState          ( const bxGdiBlendState state ) = 0;
    virtual void setRasterState         ( const bxGdiRasterState state ) = 0;
    virtual void setScissorRects        ( const bxGdiRect* rects, int n ) = 0;
    virtual void setTopology            ( int topology ) = 0;

    virtual void changeToMainFramebuffer() = 0;
    virtual void changeRenderTargets    ( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex ) = 0;
    virtual void clearBuffers           ( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth ) = 0;

    virtual void draw                   ( unsigned numVertices, unsigned startIndex ) = 0;
    virtual void drawIndexed            ( unsigned numIndices , unsigned startIndex, unsigned baseVertex ) = 0;
    virtual void drawInstanced          ( unsigned numVertices, unsigned startIndex, unsigned numInstances ) = 0;
    virtual void drawIndexedInstanced   ( unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex ) = 0;

    virtual unsigned char* map          ( bxGdiResource resource, int offsetInBytes, int mapType = bx::gdi::eMAP_WRITE ) = 0;
    virtual void unmap                  ( bxGdiResource resource ) = 0;

    virtual unsigned char* mapVertices  ( bxGdiVertexBuffer vbuffer, int firstElement, int numElements, int mapType ) = 0;
    virtual unsigned char* mapIndices   ( bxGdiIndexBuffer ibuffer, int firstElement, int numElements, int mapType ) = 0;
    virtual void unmapVertices          ( bxGdiVertexBuffer vbuffer ) = 0;
    virtual void unmapIndices           ( bxGdiIndexBuffer ibuffer ) = 0;
    virtual void updateCBuffer          ( bxGdiBuffer cbuffer, const void* data ) = 0;
    virtual void updateTexture          ( bxGdiTexture texture, const void* data ) = 0;
    virtual void swap                   () = 0;
    virtual void generateMipmaps        ( bxGdiTexture texture ) = 0;

    virtual bxGdiTexture backBufferTexture() = 0;
};

namespace bxGdi
{
    unsigned char* buffer_map( bxGdiContextBackend* ctx, bxGdiBuffer buffer, int firstElement, int numElements, int mapType = bx::gdi::eMAP_WRITE );
    unsigned char* bufferMap( bxGdiContextBackend* ctx, bxGdiBuffer buffer, u32 offset, u32 size, int mapType = bx::gdi::eMAP_WRITE );
}///


namespace bx{
namespace gdi{

int  backendStartup( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen );
void backendShutdown( bxGdiDeviceBackend** dev );
   
}}///



namespace bx{
namespace gdi{

void startup( uptr hWnd, int winWidth, int winHeight, int fullScreen );
void shutdown();

namespace frame
{
    void begin( CommandQueue** cmdQueue );
    void end( CommandQueue** cmdQueue );
}////

namespace create
{
    VertexBuffer   vertexBuffer  ( const VertexBufferDesc& desc, u32 numElements, const void* data = 0 );
    IndexBuffer    indexBuffer   ( int dataType, u32 numElements, const void* data = 0 );
    ConstantBuffer constantBuffer( u32 sizeInBytes );
    BufferRO       bufferRO      ( int numElements, bxGdiFormat format, unsigned cpuAccessFlag, unsigned gpuAccessFlag );
    //BufferRW createBufferRW( int numElements, bxGdiFormat format, unsigned bindFlags, unsigned cpuAccessFlag, unsigned gpuAccessFlag );

    Shader shader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bx::gdi::ShaderReflection* reflection = 0 );
    Shader shader( int stage, const void* codeBlob, size_t codeBlobSizee, bx::gdi::ShaderReflection* reflection = 0 );

    TextureRO    texture       ( const void* dataBlob, size_t dataBlobSize );
    TextureRW    texture1D     ( int w, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureRW    texture2D     ( int w, int h, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureDepth texture2Ddepth( int w, int h, int mips, bx::gdi::EDataType dataType );
    Sampler      sampler       ( const SamplerDesc& desc );

    InputLayout inputLayout( const VertexBufferDesc* blocks, int nblocks, Shader vertex_shader );
    BlendState  blendState( bxGdiHwStateDesc::Blend blend );
    DepthState  depthState( bxGdiHwStateDesc::Depth depth );
    RasterState rasterState( bxGdiHwStateDesc::Raster raster );

}///
namespace release
{
    void vertexBuffer  ( VertexBuffer* id );
    void indexBuffer   ( IndexBuffer* id );
    void inputLayout   ( InputLayout * id );
    void constantBuffer( ConstantBuffer* id );
    void bufferRO      ( BufferRO* id );
    void shader        ( Shader* id );
    void texture       ( TextureRO* id );
    void texture       ( TextureRW* id );
    void texture       ( TextureDepth* id );
    void sampler       ( Sampler* id );
    void blendState    ( BlendState* id );
    void depthState    ( DepthState* id );
    void rasterState   ( RasterState * id );
}///
//
namespace set
{
    void viewport      ( CommandQueue* cmdq, Viewport vp );
    void vertexBuffers ( CommandQueue* cmdq, VertexBuffer* vbuffers, unsigned start, unsigned n );
    void indexBuffer   ( CommandQueue* cmdq, IndexBuffer ibuffer );
    void shaderPrograms( CommandQueue* cmdq, Shader* shaders, int n );
    void inputLayout   ( CommandQueue* cmdq, InputLayout ilay );

    void cbuffers   ( CommandQueue* cmdq, ConstantBuffer* cbuffers, unsigned startSlot, unsigned n, unsigned stageMask );
    void resourcesRO( CommandQueue* cmdq, ResourceRO* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void resourcesRW( CommandQueue* cmdq, ResourceRW* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void samplers   ( CommandQueue* cmdq, Sampler* samplers, unsigned startSlot, unsigned n, unsigned stageMask );

    void depthState  ( CommandQueue* cmdq, DepthState state );
    void blendState  ( CommandQueue* cmdq, BlendState state );
    void rasterState ( CommandQueue* cmdq, RasterState state );
    void scissorRects( CommandQueue* cmdq, const Rect* rects, int n );
    void topology    ( CommandQueue* cmdq, int topology );

    void changeToMainFramebuffer( CommandQueue* cmdq );
    void changeRenderTargets( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex );
}///

namespace data
{
    unsigned char* map( CommandQueue* cmdq, Resource resource, int offsetInBytes, int mapType = bx::gdi::eMAP_WRITE );
    void           unmap( CommandQueue* cmdq, Resource resource );

    unsigned char* map( CommandQueue* cmdq, VertexBuffer vbuffer, int firstElement, int numElements, int mapType );
    unsigned char* map( CommandQueue* cmdq, IndexBuffer ibuffer, int firstElement, int numElements, int mapType );

    void updateCBuffer( CommandQueue* cmdq, ConstantBuffer cbuffer, const void* data );
    void updateTexture( CommandQueue* cmdq, TextureRW texture, const void* data );
}///

namespace submit
{
    void draw                ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex );
    void drawIndexed         ( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned baseVertex );
    void drawInstanced       ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex, unsigned numInstances );
    void drawIndexedInstanced( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned numInstances, unsigned baseVertex );
}///

namespace misc
{
    void clearState( CommandQueue* cmdq );
    void clearBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, float rgbad[5], int flag_color, int flag_depth );

    void swap( CommandQueue* cmdq );
    void generateMipmaps( CommandQueue* cmdq, TextureRW texture );
    TextureRW backBufferTexture( CommandQueue* cmdq );
}///

}}///

