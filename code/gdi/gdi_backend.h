#pragma once

#include <util/type.h>
#include <util/debug.h>

namespace bxGdi
{
    enum EStage
    {
        eSTAGE_VERTEX = 0,
        eSTAGE_PIXEL,
        eSTAGE_COMPUTE,
        eSTAGE_COUNT,
        eDRAW_STAGES_COUNT = eSTAGE_PIXEL + 1,

        eALL_STAGES_MASK = 0xFF,

        eSTAGE_MASK_VERTEX  = BIT_OFFSET( eSTAGE_VERTEX ),
        eSTAGE_MASK_PIXEL   = BIT_OFFSET( eSTAGE_PIXEL    ),
        eSTAGE_MASK_COMPUTE = BIT_OFFSET( eSTAGE_COMPUTE  ),
    };
    static const char* stageName[eSTAGE_COUNT] =
    {
        "vertex",
        "pixel",
        "compute",
    };

    enum EBindMask
    {
        eBIND_NONE = 0,
        eBIND_VERTEX_BUFFER    = BIT_OFFSET(0),
        eBIND_INDEX_BUFFER     = BIT_OFFSET(1),    
        eBIND_CONSTANT_BUFFER  = BIT_OFFSET(2), 
        eBIND_SHADER_RESOURCE  = BIT_OFFSET(3), 
        eBIND_STREAM_OUTPUT    = BIT_OFFSET(4),   
        eBIND_RENDER_TARGET    = BIT_OFFSET(5),   
        eBIND_DEPTH_STENCIL    = BIT_OFFSET(6),   
        eBIND_UNORDERED_ACCESS = BIT_OFFSET(7),    
    };

    enum EDataType
    {
        eTYPE_UNKNOWN = 0,
        eTYPE_BYTE,
        eTYPE_UBYTE,
        eTYPE_SHORT,
        eTYPE_USHORT,
        eTYPE_INT,
        eTYPE_UINT,
        eTYPE_FLOAT,
        eTYPE_DOUBLE,
        eTYPE_DEPTH16,
        eTYPE_DEPTH24_STENCIL8,
        eTYPE_DEPTH32F,
    };
    static const int typeStride[] = 
    {
        0, //
        1, //eBYTE,
        1, //eUBYTE,
        2, //eSHORT,
        2, //eUSHORT,
        4, //eINT,
        4, //eUINT,
        4, //eFLOAT,
        8, //eDOUBLE,
        2, //eDEPTH16,
        4, //eDEPTH24_STENCIL8,
        4, //eDEPTH32F,
    };

    enum ECpuAccess
    {
        eCPU_READ = BIT_OFFSET(0),
        eCPU_WRITE = BIT_OFFSET(1),
    };
    enum EMapType
    {
        eMAP_WRITE = 0,
        eMAP_NO_DISCARD,
    };

    enum ETopology
    {
        ePOINTS = 0,
        eLINES,
        eLINE_STRIP,
        eTRIANGLES,
        eTRIANGLE_STRIP,
    };

    enum EVertexSlot
    {
        eSLOT_POSITION = 0,	
        eSLOT_BLENDWEIGHT,
        eSLOT_NORMAL,	
        eSLOT_COLOR0,		
        eSLOT_COLOR1,		
        eSLOT_FOGCOORD,	
        eSLOT_PSIZE,		
        eSLOT_BLENDINDICES,
        eSLOT_TEXCOORD0,	
        eSLOT_TEXCOORD1,
        eSLOT_TEXCOORD2,
        eSLOT_TEXCOORD3,
        eSLOT_TEXCOORD4,
        eSLOT_TEXCOORD5,
        eSLOT_TANGENT,
        eSLOT_BINORMAL,
        eSLOT_COUNT,
    };
    static const char* slotName[eSLOT_COUNT] = 
    {
        "POSITION",	
        "BLENDWEIGHT",
        "NORMAL",	
        "COLOR0",		
        "COLOR1",		
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
    static const int slotSemanticIndex[eSLOT_COUNT] = 
    {
        0, //ePOSITION = 0
        0, //eBLENDWEIGHT,
        0, //eNORMAL,	
        0, //eCOLOR0,	
        1, //eCOLOR1,	
        0, //eFOGCOORD,	
        0, //ePSIZE,		
        0, //eBLENDINDICES
        0, //eTEXCOORD0,	
        1, //eTEXCOORD1,
        2, //eTEXCOORD2,
        3, //eTEXCOORD3,
        4, //eTEXCOORD4,
        5, //eTEXCOORD5,
        0, //eTANGENT,
    };
    EVertexSlot vertexSlotFromString( const char* n );
    
    //////////////////////////////////////////////////////////////////////
    /// hwState
	enum EDepthFunc
	{
		eDEPTH_FUNC_NEVER = 0,
		eDEPTH_FUNC_LESS	  ,
		eDEPTH_FUNC_EQUAL	  ,
		eDEPTH_FUNC_LEQUAL	  ,
		eDEPTH_FUNC_GREATER  ,
		eDEPTH_FUNC_NOTEQUAL ,
		eDEPTH_FUNC_GEQUAL	  ,
		eDEPTH_FUNC_ALWAYS	  ,
	};

	enum EBlendFactor
	{
		eBLEND_ZERO				    = 0,
		eBLEND_ONE				    ,
		eBLEND_SRC_COLOR			,
		eBLEND_ONE_MINUS_SRC_COLOR,
		eBLEND_DST_COLOR			,
		eBLEND_ONE_MINUS_DST_COLOR,
		eBLEND_SRC_ALPHA			,
		eBLEND_ONE_MINUS_SRC_ALPHA,
		eBLEND_DST_ALPHA			,
		eBLEND_ONE_MINUS_DST_ALPHA,
		eBLEND_SRC_ALPHA_SATURATE	,
	};

    enum EBlendEquation
    {
        eBLEND_ADD = 0,
        eBLEND_SUB,
        eBLEND_REVERSE_SUB,
        eBLEND_MIN,
        eBLEND_MAX,
    };

	enum ECulling
	{
		eCULL_NONE = 0,
		eCULL_BACK,
		eCULL_FRONT,
	};

	enum EFillmode
	{
		eFILL_SOLID = 0,
		eFILL_WIREFRAME,
	};

	enum EColorMask
	{
		eCOLOR_MASK_NONE  = 0,
		eCOLOR_MASK_RED   = BIT_OFFSET(0),
		eCOLOR_MASK_GREEN = BIT_OFFSET(1),
		eCOLOR_MASK_BLUE  = BIT_OFFSET(2),
		eCOLOR_MASK_ALPHA = BIT_OFFSET(3),
		eCOLOR_MASK_ALL   = eCOLOR_MASK_RED|eCOLOR_MASK_GREEN|eCOLOR_MASK_BLUE|eCOLOR_MASK_ALPHA,
	};

    enum ESamplerFilter
    {
        eFILTER_NEAREST = 0,
        eFILTER_LINEAR,
        eFILTER_BILINEAR,
        eFILTER_TRILINEAR,
        eFILTER_BILINEAR_ANISO,
        eFILTER_TRILINEAR_ANISO,
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
    inline int hasMipmaps( ESamplerFilter filter)	{ return (filter >= eFILTER_BILINEAR); }
    inline int hasAniso( ESamplerFilter filter)		{ return (filter >= eFILTER_BILINEAR_ANISO); }

    enum ESamplerDepthCompare
    {
        eDEPTH_CMP_NONE = 0,
        eDEPTH_CMP_GREATER,
        eDEPTH_CMP_GEQUAL,
        eDEPTH_CMP_LESS,
        eDEPTH_CMP_LEQUAL,
        eDEPTH_CMP_ALWAYS,
        eDEPTH_CMP_NEVER,
        eDEPTH_CMP_COUNT
    };

    enum EAddressMode
    {
        eADDRESS_WRAP,
        eADDRESS_CLAMP,
        eADDRESS_BORDER,
    };
    static const char* addressModeName[] = 
    {
        "WRAP",
        "CLAMP",
        "BORDER",
    };

    static const u32 cMAX_RENDER_TARGETS = 8;
    static const u32 cMAX_CBUFFERS = 6;
    static const u32 cMAX_TEXTURES = 8;
    static const u32 cMAX_SAMPLERS = 8;
    static const u32 cMAX_VERTEX_BUFFERS = 6;
    static const u32 cMAX_SHADER_MACRO = 32;

    struct ShaderReflection;
}///
struct bxGdiViewport
{
    i16 x, y;
    u16 w, h;

    bxGdiViewport() {}
    bxGdiViewport( int xx, int yy, unsigned ww, unsigned hh )
        : x(xx), y(yy), w(ww), h(hh) {}
};

union bxGdiVertexStreamBlock
{
    u16 hash;
    struct
    {
        u16 slot        : 8;
        u16 dataType    : 4;
        u16 numElements : 4;
    };
};
struct bxGdiVertexStreamDesc
{
    enum { eMAX_BLOCKS = 5 };
    bxGdiVertexStreamBlock blocks[eMAX_BLOCKS];
    i16 count;

    bxGdiVertexStreamDesc();
    void addBlock( bxGdi::EVertexSlot slot, bxGdi::EDataType type, int numElements );
};
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

struct bxGdiFormat
{
    u8 type;
    u8 numElements : 6;
    u8 normalized : 1;
    u8 srgb : 1;

    bxGdiFormat( bxGdi::EDataType t, u8 nElem, u8 norm = 0, u8 s = 0 )
        : type( t ), numElements( nElem ), normalized( norm ), srgb(s)
    {}

    bxGdiFormat()
        : type(0), numElements(0), normalized(0), srgb(0) 
    {}
};

union bxGdiSamplerDesc
{
    bxGdiSamplerDesc()
        : filter( bxGdi::eFILTER_TRILINEAR_ANISO )
        , addressU( bxGdi::eADDRESS_WRAP )
        , addressV( bxGdi::eADDRESS_WRAP )
        , addressT( bxGdi::eADDRESS_WRAP )
        , depthCmpMode( bxGdi::eDEPTH_CMP_NONE )
        , aniso( 16 )
    {}
    bxGdiSamplerDesc( bxGdi::ESamplerFilter f, bxGdi::EAddressMode u, bxGdi::EAddressMode v, bxGdi::EAddressMode t, bxGdi::ESamplerDepthCompare cmp, u32 a )
        : filter( u8(f) )
        , addressU( u8(u) )
        , addressV( u8(v) )
        , addressT( u8(t) )
        , depthCmpMode( u8(cmp) )
        , aniso( u8(a) )
    {}
    bxGdiSamplerDesc( bxGdi::ESamplerFilter f, bxGdi::EAddressMode uvt = bxGdi::eADDRESS_CLAMP, bxGdi::ESamplerDepthCompare cmp = bxGdi::eDEPTH_CMP_NONE, u32 a = 16 )
        : filter( u8(f) )
        , addressU( u8(uvt) )
        , addressV( u8(uvt) )
        , addressT( u8(uvt) )
        , depthCmpMode( u8(cmp) )
        , aniso( u8(a) )
    {}

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

struct bxGdiHwState
{
    union Blend
    {
	    u32 key;
	    struct  
	    {
		    u64 enable           : 1;
		    u64 color_mask       : 4;
            u64 equation         : 4;
            u64 srcFactor       : 4;
            u64 dstFactor       : 4;
		    u64 srcFactorAlpha : 4;
		    u64 dstFactorAlpha : 4;
	    };
    };

    union Depth
    {
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
	    u32 key;
	    struct  
	    {
		    u32 cullMode        : 2;
		    u32 fillMode        : 2;
		    u32 multisample      : 1;
		    u32 antialiasedLine : 1;
		    u32 scissor          : 1;
	    };
    };

    bxGdiHwState()
	{
		blend.enable = 0;
		blend.color_mask = bxGdi::eCOLOR_MASK_ALL;
		blend.srcFactorAlpha = bxGdi::eBLEND_ONE;
		blend.dstFactorAlpha = bxGdi::eBLEND_ZERO;
        blend.srcFactor = bxGdi::eBLEND_ONE;
        blend.dstFactor = bxGdi::eBLEND_ZERO;
        blend.equation = bxGdi::eBLEND_ADD;

		depth.function = bxGdi::eDEPTH_FUNC_LEQUAL;
		depth.test = 1;
		depth.write = 1;

		raster.cullMode = bxGdi::eCULL_BACK;
		raster.fillMode = bxGdi::eFILL_SOLID;
		raster.multisample = 1;
		raster.antialiasedLine = 0;
		raster.scissor = 0;
	}
	Blend blend;
	Depth depth;
	Raster raster;
};
inline bool operator == ( const bxGdiHwState&  a, const bxGdiHwState& b )
{
    return a.blend.key == b.blend.key &&
           a.depth.key == b.depth.key &&
           a.raster.key == b.raster.key;
}
inline bool operator != ( const bxGdiHwState&  a, const bxGdiHwState& b )
{
    return !(a == b);
}

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
union bxGdiShader
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

//namespace bxGdi
//{
//    inline bxGdiSamplerDesc nullSamplerDesc() 
//    { 
//        bxGdiSamplerDesc sd; 
//        sd.key = u64(~0); 
//        return sd; 
//    }
//    
//    inline bxGdiIndexBuffer nullIndexBuffer() 
//    { 
//        bxGdiIndexBuffer buff;
//        buff.id = 0;
//        buff.dataType = 0;
//        buff.numElements = 0;
//        return buff;
//    }
//    inline bxGdiShader nullShader()
//    {
//        bxGdiShader shader = { 0 };
//        return shader;
//    }
//    inline bxGdiTexture nullTexture()
//    {
//        bxGdiTexture texture;
//        texture.id = 0;
//        texture.dx.viewSH = 0;
//        texture.dx.viewRT = 0;
//        texture.dx.viewDS = 0;
//        texture.dx.viewUA = 0;
//        return texture;
//    }
//    inline bxGdiSampler nullSampler()
//    {
//        bxGdiSampler s = { 0 };
//        return s;
//    }
//    inline bxGdiBuffer nullBuffer()
//    {
//        bxGdiBuffer buffer = { 0 };
//        return buffer;
//    }
//}//

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
    virtual bxGdiBlendState  createBlendState( bxGdiHwState::Blend blend ) = 0;
    virtual bxGdiDepthState  createDepthState( bxGdiHwState::Depth depth ) = 0;
    virtual bxGdiRasterState createRasterState( bxGdiHwState::Raster raster ) = 0;

    virtual void releaseVertexBuffer( bxGdiVertexBuffer* id ) = 0;
    virtual void releaseIndexBuffer( bxGdiIndexBuffer* id ) = 0;
    virtual void releaseBuffer( bxGdiBuffer* id ) = 0;
    virtual void releaseShader( bxGdiShader* id ) = 0;
    virtual void releaseTexture( bxGdiTexture* id ) = 0;
    virtual void releaseInputLayout( bxGdiInputLayout * id ) = 0;
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
};

namespace bxGdi
{
    int  backendStartup( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen );
    void backendShutdown( bxGdiDeviceBackend** dev );
   
}///
