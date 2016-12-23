#pragma once

#include <util/type.h>
#include "util/viewport.h"

namespace bx{ namespace rdi {

namespace EStage
{
    enum Enum
    {
        VERTEX = 0,
        PIXEL,
        COMPUTE,
        COUNT,
        DRAW_STAGES_COUNT = PIXEL + 1,

        ALL_STAGES_MASK = 0xFF,

        VERTEX_MASK  = BIT_OFFSET( VERTEX ),
        PIXEL_MASK   = BIT_OFFSET( PIXEL ),
        COMPUTE_MASK = BIT_OFFSET( COMPUTE ),
    };

    static const char* name[COUNT] =
    {
        "vertex",
        "pixel",
        "compute",
    };
};

namespace EBindMask
{
    enum Enum
    {
        NONE = 0,
        VERTEX_BUFFER = BIT_OFFSET( 0 ),
        INDEX_BUFFER = BIT_OFFSET( 1 ),
        CONSTANT_BUFFER = BIT_OFFSET( 2 ),
        SHADER_RESOURCE = BIT_OFFSET( 3 ),
        STREAM_OUTPUT = BIT_OFFSET( 4 ),
        RENDER_TARGET = BIT_OFFSET( 5 ),
        DEPTH_STENCIL = BIT_OFFSET( 6 ),
        UNORDERED_ACCESS = BIT_OFFSET( 7 ),
    };
};

namespace EDataType
{
    enum Enum
    {
        UNKNOWN = 0,
        BYTE,
        UBYTE,
        SHORT,
        USHORT,
        INT,
        UINT,
        FLOAT,
        DOUBLE,
        DEPTH16,
        DEPTH24_STENCIL8,
        DEPTH32F,

        COUNT,
    };
    static const unsigned stride[] =
    {
        0, //
        1, //BYTE,
        1, //UBYTE,
        2, //SHORT,
        2, //USHORT,
        4, //INT,
        4, //UINT,
        4, //FLOAT,
        8, //DOUBLE,
        2, //DEPTH16,
        4, //DEPTH24_STENCIL8,
        4, //DEPTH32F,
    };
    static const char* name[] =
    {
        "none",
        "byte",
        "ubyte",
        "short",
        "ushort",
        "int",
        "uint",
        "float",
        "double",
        "depth16",
        "depth24_stencil8",
        "depth32F",
    };
    Enum FromName( const char* name );
    Enum FindBaseType( const char* name );

};


struct Format
{
    u8 type;
    u8 numElements : 6;
    u8 normalized : 1;
    u8 srgb : 1;

    Format()
        : type(EDataType::UNKNOWN)
        , numElements(0), normalized(0), srgb(0)
    {}

    Format( EDataType::Enum dt, u8 ne ) { type = dt; numElements = ne; normalized = 0; srgb = 0; }
    Format& Normalized( u32 onOff ) { normalized = onOff; return *this; }
    Format& Srgb( u32 onOff ) { srgb = onOff; return *this; }
    inline u32 ByteWidth() const { return EDataType::stride[type] * numElements; }
};


namespace ECpuAccess
{
    enum Enum
    {
        READ = BIT_OFFSET( 0 ),
        WRITE = BIT_OFFSET( 1 ),
    };
};
namespace EGpuAccess
{
    enum Enum
    {
        READ = BIT_OFFSET( 1 ),
        WRITE = BIT_OFFSET( 2 ),
        READ_WRITE = READ | WRITE,
    };
};

namespace EMapType
{
    enum Enum
    {
        WRITE = 0,
        NO_DISCARD,
    };
};

namespace ETopology
{
    enum Enum
    {
        POINTS = 0,
        LINES,
        LINE_STRIP,
        TRIANGLES,
        TRIANGLE_STRIP,
    };
};

namespace EVertexSlot
{
    enum Enum
    {
        POSITION = 0,
        BLENDWEIGHT,
        NORMAL,
        COLOR0,
        COLOR1,
        FOGCOORD,
        PSIZE,
        BLENDINDICES,
        TEXCOORD0,
        TEXCOORD1,
        TEXCOORD2,
        TEXCOORD3,
        TEXCOORD4,
        TEXCOORD5,
        TANGENT,
        BINORMAL,
        COUNT,
    };

    static const char* name[EVertexSlot::COUNT] =
    {
        "POSITION",
        "BLENDWEIGHT",
        "NORMAL",
        "COLOR",
        "COLOR",
        "FOGCOORD",
        "PSIZE",
        "BLENDINDICES",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TANGENT",
        "BINORMAL",
    };
    static const int semanticIndex[EVertexSlot::COUNT] =
    {
        0, //POSITION = 0
        0, //BLENDWEIGHT,
        0, //NORMAL,	
        0, //COLOR0,	
        1, //COLOR1,	
        0, //FOGCOORD,	
        0, //PSIZE,		
        0, //BLENDINDICES
        0, //TEXCOORD0,	
        1, //TEXCOORD1,
        2, //TEXCOORD2,
        3, //TEXCOORD3,
        4, //TEXCOORD4,
        5, //TEXCOORD5,
        0, //TANGENT,
    };
    EVertexSlot::Enum FromString( const char* n );

};


//////////////////////////////////////////////////////////////////////
/// hwState
namespace EDepthFunc
{
    enum Enum
    {
        NEVER = 0,
        LESS,
        EQUAL,
        LEQUAL,
        GREATER,
        NOTEQUAL,
        GEQUAL,
        ALWAYS,
    };
};


namespace EBlendFactor
{
    enum Enum
    {
        ZERO = 0,
        ONE,
        SRC_COLOR,
        ONE_MINUS_SRC_COLOR,
        DST_COLOR,
        ONE_MINUS_DST_COLOR,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA,
        SRC_ALPHA_SATURATE,
    };
};

namespace EBlendEquation
{
    enum Enum
    {
        ADD = 0,
        SUB,
        REVERSE_SUB,
        MIN,
        MAX,
    };
};

namespace ECullMode
{
    enum Enum
    {
        NONE = 0,
        BACK,
        FRONT,
    };
};

namespace EFillMode
{
    enum Enum
    {
        SOLID = 0,
        WIREFRAME,
    };
};

namespace EColorMask
{
    enum Enum
    {
        NONE = 0,
        RED = BIT_OFFSET( 0 ),
        GREEN = BIT_OFFSET( 1 ),
        BLUE = BIT_OFFSET( 2 ),
        ALPHA = BIT_OFFSET( 3 ),
        ALL = RED | GREEN | BLUE | ALPHA,
    };
};

namespace ESamplerFilter
{
    enum Enum
    {
        NEAREST = 0,
        LINEAR,
        BILINEAR,
        TRILINEAR,
        BILINEAR_ANISO,
        TRILINEAR_ANISO,
    };

    static const char* name[] =
    {
        "NEAREST",
        "LINEAR",
        "BILINEAR",
        "TRILINEAR",
        "BILINEAR_ANISO",
        "TRILINEAR_ANISO",
    };
    inline int hasMipmaps( Enum filter )	{ return ( filter >= BILINEAR ); }
    inline int hasAniso  ( Enum filter )    { return ( filter >= BILINEAR_ANISO ); }
};


namespace ESamplerDepthCmp
{
    enum Enum
    {
        NONE = 0,
        GREATER,
        GEQUAL,
        LESS,
        LEQUAL,
        ALWAYS,
        NEVER,
        COUNT
    };
};

namespace EAddressMode
{
    enum Enum
    {
        WRAP,
        CLAMP,
        BORDER,
    };

    static const char* name[] =
    {
        "WRAP",
        "CLAMP",
        "BORDER",
    };
};


static const u32 cMAX_RENDER_TARGETS = 8;
static const u32 cMAX_CBUFFERS = 6;
static const u32 cMAX_TEXTURES = 8;
static const u32 cMAX_RESOURCES_RO = 8;
static const u32 cMAX_RESOURCES_RW = 6;
static const u32 cMAX_SAMPLERS = 8;
static const u32 cMAX_VERTEX_BUFFERS = 6;
static const u32 cMAX_SHADER_MACRO = 32;

struct ShaderReflection;

//////////////////////////////////////////////////////////////////////////
union SamplerDesc
{
    SamplerDesc()
        : filter( ESamplerFilter::TRILINEAR_ANISO )
        , addressU( EAddressMode::WRAP )
        , addressV( EAddressMode::WRAP )
        , addressT( EAddressMode::WRAP )
        , depthCmpMode( ESamplerDepthCmp::NONE )
        , aniso( 16 )
    {}

    SamplerDesc& Filter  ( ESamplerFilter::Enum f )     { filter = f; return *this; }
    SamplerDesc& Address ( EAddressMode::Enum u )       { addressU = addressV = addressT = u; return *this;  }
    SamplerDesc& Address ( EAddressMode::Enum u, 
                           EAddressMode::Enum v, 
                           EAddressMode::Enum t )       { addressU = u;  addressV = v;  addressT = t; return *this; }
    SamplerDesc& DepthCmp( ESamplerDepthCmp::Enum dc )  { depthCmpMode = dc; return *this; }
    SamplerDesc& Aniso   ( u8 a )                       { aniso = a; return *this; }

    u64 key;
    struct
    {
        u8 filter;
        u8 addressU;
        u8 addressV;
        u8 addressT;
        u8 depthCmpMode;
        u8 aniso;
        u16 _padding;
    };
};

struct HardwareStateDesc
{
    union Blend
    {
        Blend& Enable     ()                                                 { enable = 1; return *this; }
        Blend& Disable    ()                                                 { enable = 0; return *this; }
        Blend& ColorMask  ( EColorMask::Enum m )                             { color_mask = m; return *this; }
        Blend& Factor     ( EBlendFactor::Enum src, EBlendFactor::Enum dst ) { srcFactor = src; dstFactor = dst; return *this; }
        Blend& FactorAlpha( EBlendFactor::Enum src, EBlendFactor::Enum dst ) { srcFactorAlpha = src; dstFactorAlpha = dst; return *this; }
        Blend& Equation   ( EBlendEquation::Enum eq )                        { equation = eq; return *this; }

        u32 key;
        struct
        {
            u64 enable : 1;
            u64 color_mask : 4;
            u64 equation : 4;
            u64 srcFactor : 4;
            u64 dstFactor : 4;
            u64 srcFactorAlpha : 4;
            u64 dstFactorAlpha : 4;
        };
    };

    union Depth
    {
        Depth& Function( EDepthFunc::Enum f ) { function = f; return *this; }
        Depth& Test    ( u8 onOff )           { test = onOff; return *this; }
        Depth& Write   ( u8 onOff )           { write = onOff; return *this; }

        u32 key;
        struct
        {
            u16 function;
            u8  test;
            u8	write;
        };
    };

    union Raster
    {
        Raster& CullMode        ( ECullMode::Enum c )  { cullMode = c; return *this; }
        Raster& FillMode        ( EFillMode::Enum f ) { fillMode = f; return *this; }
        Raster& Multisample     ( u32 onOff )         { multisample = onOff; return *this; }
        Raster& AntialiassedLine( u32 onOff )         { antialiasedLine = onOff; return *this; }
        Raster& Scissor         ( u32 onOff )         { scissor = onOff; return *this; }
            
        u32 key;
        struct
        {
            u32 cullMode : 2;
            u32 fillMode : 2;
            u32 multisample : 1;
            u32 antialiasedLine : 1;
            u32 scissor : 1;
        };
    };

    HardwareStateDesc()
    {
        blend
            .Disable()
            .ColorMask( EColorMask::ALL )
            .FactorAlpha( EBlendFactor::ONE, EBlendFactor::ZERO )
            .Factor( EBlendFactor::ONE, EBlendFactor::ZERO )
            .Equation( EBlendEquation::ADD );
            
        depth.Function( EDepthFunc::LEQUAL ).Test( 1 ).Write( 1 );

        raster.CullMode( ECullMode::BACK ).FillMode( EFillMode::SOLID ).Multisample( 1 ).AntialiassedLine( 1 ).Scissor( 0 );
    }
    Blend blend;
    Depth depth;
    Raster raster;
};

inline bool operator == ( const HardwareStateDesc&  a, const HardwareStateDesc& b )
{
    return a.blend.key == b.blend.key &&
        a.depth.key == b.depth.key &&
        a.raster.key == b.raster.key;
}
inline bool operator != ( const HardwareStateDesc&  a, const HardwareStateDesc& b )
{
    return !( a == b );
}

typedef bx::gfx::Viewport Viewport;

//////////////////////////////////////////////////////////////////////////
union VertexBufferDesc
{
    VertexBufferDesc() {}
    VertexBufferDesc( EVertexSlot::Enum s ) { slot = s; }
    VertexBufferDesc& DataType( EDataType::Enum dt, u32 ne ) 
    { 
        dataType = dt; 
        numElements = ne;
        return *this; 
    }
    VertexBufferDesc& Normalized() { typeNorm = 1; return *this; }

    inline u32 ByteWidth() const { return EDataType::stride[dataType] * numElements; }

    u16 hash = 0;
    struct
    {
        u16 slot : 7;
        u16 dataType : 4;
        u16 typeNorm : 1;
        u16 numElements : 4;
    };
};

struct VertexLayout
{
    VertexBufferDesc descs[cMAX_VERTEX_BUFFERS] = {};
    u32 count = 0;

    u32 InputMask() const
    {
        u32 mask = 0;
        for( u32 i = 0; i < count; ++i )
            mask |= 1 << descs[i].slot;
        return mask;
    }
};

}}///


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


namespace bx { namespace rdi {

struct Resource
{
    union
    {
        uptr id = 0;
        ID3D11Resource*  resource;
        ID3D11Texture1D* texture1D;
        ID3D11Texture2D* texture2D;
        ID3D11Texture3D* texture3D;
        ID3D11Buffer*    buffer;
    };
};

struct ResourceRO : Resource
{
    ID3D11ShaderResourceView* viewSH = nullptr;
};
struct ResourceRW : ResourceRO
{
    ID3D11UnorderedAccessView* viewUA = nullptr;
};



struct VertexBuffer : Resource
{
    VertexBufferDesc desc = {};
    u32 numElements = 0;
};
struct IndexBuffer : Resource
{
    u32 dataType = 0;
    u32 numElements = 0;
};

struct ConstantBuffer : Resource
{
    u32 size_in_bytes = 0;
};

struct BufferRO : ResourceRO
{
    u32 sizeInBytes = 0;
    u32 bind_flags = 0;
    Format format = {};
};
struct BufferRW : ResourceRW
{
    u32 sizeInBytes = 0;
    u32 bind_flags = 0;
    Format format;
};

struct TextureInfo
{
    u16 width = 0;
    u16 height = 0;
    u16 depth = 0;
    u8 mips = 0;
    Format format = {};
    
};

struct TextureRO : ResourceRO
{
    TextureInfo info;
};

struct TextureRW : ResourceRW
{
    ID3D11RenderTargetView* viewRT = nullptr;
    TextureInfo info;
};

struct TextureDepth : ResourceRW
{
    ID3D11DepthStencilView* viewDS = nullptr;
    TextureInfo info;
};

struct Shader
{
    union
    {
        uptr id = 0;
        union
        {
            ID3D11DeviceChild*      object;
            ID3D11VertexShader*     vertex;
            ID3D11PixelShader*      pixel;
            ID3D11ComputeShader*    compute;
        };
    };

    void* inputSignature = nullptr;
    i16 stage = -1;
    u16 vertexInputMask = 0;
};

struct ShaderPass
{
    ID3D11VertexShader* vertex = nullptr;
    ID3D11PixelShader* pixel = nullptr;
    void* input_signature = nullptr;
    u32 vertex_input_mask = 0;
};
struct ShaderPassCreateInfo
{
    void* vertex_bytecode = nullptr;
    void* pixel_bytecode = nullptr;
    size_t vertex_bytecode_size = 0;
    size_t pixel_bytecode_size = 0;
    ShaderReflection* reflection = nullptr;
};

union InputLayout
{
    uptr id = 0;
    ID3D11InputLayout* layout;
};

union BlendState
{
    uptr id = 0;
    ID3D11BlendState* state;
};
union DepthState
{
    uptr id = 0;
    ID3D11DepthStencilState* state;
};
union RasterState
{
    uptr id = 0;
    ID3D11RasterizerState* state;
};

struct HardwareState
{
    ID3D11BlendState* blend = nullptr;
    ID3D11DepthStencilState* depth = nullptr;
    ID3D11RasterizerState* raster = nullptr;
};

union Sampler
{
    uptr id = 0;
    ID3D11SamplerState* state;
};

struct Rect
{
    i32 left, top, right, bottom;

    Rect() {}
    Rect( int l, int t, int r, int b )
        : left( l ), top( t ), right( r ), bottom( b )
    {}
};
//
struct CommandQueue;

}}///


namespace bx{ namespace rdi{

void Startup( uptr hWnd, int winWidth, int winHeight, int fullScreen );
void Shutdown();

namespace frame
{
    void Begin( CommandQueue** cmdQueue );
    void End( CommandQueue** cmdQueue );
}////

namespace device
{
    VertexBuffer   CreateVertexBuffer  ( const VertexBufferDesc& desc, u32 numElements, const void* data = 0 );
    IndexBuffer    CreateIndexBuffer   ( EDataType::Enum dataType, u32 numElements, const void* data = 0 );
    ConstantBuffer CreateConstantBuffer( u32 sizeInBytes, const void* data = nullptr );
    BufferRO       CreateBufferRO      ( int numElements, Format format, unsigned cpuAccessFlag, unsigned gpuAccessFlag );
    //BufferRW createBufferRW( int numElements, bxGdiFormat format, unsigned bindFlags, unsigned cpuAccessFlag, unsigned gpuAccessFlag );

    //Shader CreateShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, ShaderReflection* reflection = 0 );
    //Shader CreateShader( int stage, const void* codeBlob, size_t codeBlobSizee, ShaderReflection* reflection = 0 );
    ShaderPass CreateShaderPass( const ShaderPassCreateInfo& info );

    TextureRO    CreateTextureFromDDS( const void* dataBlob, size_t dataBlobSize );
    TextureRO    CreateTextureFromHDR( const void* dataBlob, size_t dataBlobSize );
    TextureRW    CreateTexture1D     ( int w, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureRW    CreateTexture2D     ( int w, int h, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureDepth CreateTexture2Ddepth( int w, int h, int mips, EDataType::Enum dataType );
    Sampler      CreateSampler       ( const SamplerDesc& desc );

    InputLayout CreateInputLayout( const VertexBufferDesc* blocks, int nblocks, Shader vertex_shader );
    InputLayout CreateInputLayout( const VertexLayout vertexLayout, ShaderPass shaderPass );
    //BlendState  CreateBlendState( HardwareStateDesc::Blend blend );
    //DepthState  CreateDepthState( HardwareStateDesc::Depth depth );
    //RasterState CreateRasterState( HardwareStateDesc::Raster raster );
    HardwareState CreateHardwareState( HardwareStateDesc desc );

    void DestroyVertexBuffer  ( VertexBuffer* id );
    void DestroyIndexBuffer   ( IndexBuffer* id );
    void DestroyInputLayout   ( InputLayout * id );
    void DestroyConstantBuffer( ConstantBuffer* id );
    void DestroyBufferRO      ( BufferRO* id );
    //void DestroyShader        ( Shader* id );
    void DestroyShaderPass    ( ShaderPass* id );
    void DestroyTexture       ( TextureRO* id );
    void DestroyTexture       ( TextureRW* id );
    void DestroyTexture       ( TextureDepth* id );
    void DestroySampler       ( Sampler* id );
    void DestroyBlendState    ( BlendState* id );
    void DestroyDepthState    ( DepthState* id );
    void DestroyRasterState   ( RasterState * id );
    void DestroyHardwareState ( HardwareState* id );
}///
//

namespace context
{
    void SetViewport      ( CommandQueue* cmdq, Viewport vp );
    void SetVertexBuffers ( CommandQueue* cmdq, VertexBuffer* vbuffers, unsigned start, unsigned n );
    void SetIndexBuffer   ( CommandQueue* cmdq, IndexBuffer ibuffer );
    void SetShaderPrograms( CommandQueue* cmdq, Shader* shaders, int n );
    void SetShaderPass    ( CommandQueue* cmdq, ShaderPass pass );
    void SetInputLayout   ( CommandQueue* cmdq, InputLayout ilay );

    void SetCbuffers   ( CommandQueue* cmdq, ConstantBuffer* cbuffers, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetResourcesRO( CommandQueue* cmdq, ResourceRO* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetResourcesRW( CommandQueue* cmdq, ResourceRW* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetSamplers   ( CommandQueue* cmdq, Sampler* samplers, unsigned startSlot, unsigned n, unsigned stageMask );

    void SetDepthState  ( CommandQueue* cmdq, DepthState state );
    void SetBlendState  ( CommandQueue* cmdq, BlendState state );
    void SetRasterState ( CommandQueue* cmdq, RasterState state );
    void SetHardwareState( CommandQueue* cmdq, HardwareState hwstate );
    void SetScissorRects( CommandQueue* cmdq, const Rect* rects, int n );
    void SetTopology    ( CommandQueue* cmdq, int topology );

    void ChangeToMainFramebuffer( CommandQueue* cmdq );
    void ChangeRenderTargets( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, bool changeViewport = true );

    unsigned char* Map( CommandQueue* cmdq, Resource resource, int offsetInBytes, int mapType = EMapType::WRITE );
    void           Unmap( CommandQueue* cmdq, Resource resource );

    unsigned char* Map( CommandQueue* cmdq, VertexBuffer vbuffer, int firstElement, int numElements, int mapType );
    unsigned char* Map( CommandQueue* cmdq, IndexBuffer ibuffer, int firstElement, int numElements, int mapType );

    void UpdateCBuffer( CommandQueue* cmdq, ConstantBuffer cbuffer, const void* data );
    void UpdateTexture( CommandQueue* cmdq, TextureRW texture, const void* data );

    void Draw                ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex );
    void DrawIndexed         ( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned baseVertex );
    void DrawInstanced       ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex, unsigned numInstances );
    void DrawIndexedInstanced( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned numInstances, unsigned baseVertex );


    void ClearState( CommandQueue* cmdq );
    void ClearBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, const float rgbad[5], int flag_color, int flag_depth );
    void ClearDepthBuffer( CommandQueue* cmdq, TextureDepth depthTex, float clearValue );
    void ClearColorBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, float r, float g, float b, float a );
    inline void ClearColorBuffer( CommandQueue* cmdq, TextureRW colorTex, float r, float g, float b, float a )
    {
        ClearColorBuffers( cmdq, &colorTex, 1, r, g, b, a );
    }

    void Swap( CommandQueue* cmdq );
    void GenerateMipmaps( CommandQueue* cmdq, TextureRW texture );
    TextureRW GetBackBufferTexture( CommandQueue* cmdq );
}/// 

}}///

