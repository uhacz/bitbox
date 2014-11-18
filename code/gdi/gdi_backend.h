#pragma once

#include <util/debug.h>
#include "gdi_backend_struct.h"

namespace bxGdi
{
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
};

//////////////////////////////////////////////////////////////////////////
/// dx11 structs
struct ID3D11Resource;
struct ID3D11DeviceChild;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11InputLayout;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

struct ID3D11Texture1D;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11UnorderedAccessView;
//////////////////////////////////////////////////////////////////////////

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
    };

    u32 sizeInBytes;
    u32 bindFlags;

    bxGdiBuffer()
        : id(0)
        , sizeInBytes(0)
        , bindFlags(0)
    {}
};
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

    i32 stage;

    bxGdiShader()
        : id(0)
        , stage(-1)
    {}
};
struct bxGdiTexture
{
    union
    {
        uptr id;
        struct
        {
            union 
            {
                ID3D11Resource*   resource;
                ID3D11Texture1D* _1D;
                ID3D11Texture2D* _2D;
                ID3D11Texture3D* _3D;
            };

            ID3D11ShaderResourceView*   viewSH;
            ID3D11RenderTargetView*     viewRT;
            ID3D11DepthStencilView*     viewDS;
            ID3D11UnorderedAccessView*  viewUA;
        } dx;
    };

    i16 width;
    i16 height;
    i16 depth;
    bxGdiFormat format;

    bxGdiTexture()
        : id(0)
        , width(0), height(0), depth(0)
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

struct bxGdiContextBackend;
struct bxGdiDeviceBackend
{
    virtual ~bxGdiDeviceBackend() {}

    virtual bxGdiVertexBuffer createVertexBuffer( const bxGdiVertexStreamDesc& desc, u32 numElements, const void* data = 0 ) = 0;
    virtual bxGdiIndexBuffer  createIndexBuffer( int dataType, u32 numElements, const void* data = 0  ) = 0;
    virtual bxGdiBuffer createBuffer( u32 sizeInBytes, u32 bindFlags ) = 0;
    
    virtual bxGdiShader createShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bxGdi::ShaderReflection* reflection = 0) = 0;
    virtual bxGdiShader createShader( int stage, const void* codeBlob, size_t codeBlobSizee, bxGdi::ShaderReflection* reflection = 0  ) = 0;
    
    virtual bxGdiTexture createTexture( const void* dataBlob, size_t dataBlobSize ) = 0;
    virtual bxGdiTexture createTexture1D() = 0;
    virtual bxGdiTexture createTexture2D( int w, int h, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data ) = 0;
    virtual bxGdiTexture createTexture2Ddepth( int w, int h, int mips, bxGdi::EDataType dataType, unsigned bindFlags ) = 0;
    virtual bxGdiTexture createTexture3D() = 0;
    virtual bxGdiTexture createTextureCube() = 0;
    virtual bxGdiSampler createSampler( const bxGdiSamplerDesc& desc ) = 0;
    
    virtual bxGdiInputLayout createInputLayout( const bxGdiVertexStreamDesc* descs, int ndescs, bxGdiShader vertex_shader ) = 0;
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

    virtual void setCbuffers            ( bxGdiBuffer* cbuffers , unsigned startSlot, unsigned n, int stage ) = 0;
    virtual void setTextures            ( bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage ) = 0;
    virtual void setSamplers            ( bxGdiSampler* samplers, unsigned startSlot, unsigned n, int stage ) = 0;

    virtual void setDepthState          ( const bxGdiDepthState state ) = 0;
    virtual void setBlendState          ( const bxGdiBlendState state ) = 0;
    virtual void setRasterState         ( const bxGdiRasterState state ) = 0;

    virtual void setTopology            ( int topology ) = 0;

    virtual void changeToMainFramebuffer() = 0;
    virtual void changeRenderTargets    ( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex ) = 0;
    virtual void clearBuffers           ( bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth ) = 0;

    virtual void draw                   ( unsigned numVertices, unsigned startIndex ) = 0;
    virtual void drawIndexed            ( unsigned numIndices , unsigned startIndex, unsigned baseVertex ) = 0;
    virtual void drawInstanced          ( unsigned numVertices, unsigned startIndex, unsigned numInstances ) = 0;
    virtual void drawIndexedInstanced   ( unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex ) = 0;

    virtual unsigned char* mapVertices  ( bxGdiVertexBuffer vbuffer, int firstElement, int numElements, int mapType ) = 0;
    virtual unsigned char* mapIndices   ( bxGdiIndexBuffer ibuffer, int firstElement, int numElements, int mapType ) = 0;
    virtual void unmapVertices          ( bxGdiVertexBuffer vbuffer ) = 0;
    virtual void unmapIndices           ( bxGdiIndexBuffer ibuffer ) = 0;
    virtual void updateCBuffer          ( bxGdiBuffer cbuffer, const void* data ) = 0;
    virtual void swap                   () = 0;
    virtual void generateMipmaps        ( bxGdiTexture texture ) = 0;

    virtual bxGdiTexture backBufferTexture() = 0;

};

namespace bxGdi
{
    int  backendStartup( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen );
    void backendShutdown( bxGdiDeviceBackend** dev );
   
}///
