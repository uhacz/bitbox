#pragma once

#include "gdi_backend_type.h"
#include <util/viewport.h>

typedef bx::gfx::Viewport bxGdiViewport;

namespace bx{
namespace gdi{

	inline int equal( const bxGdiViewport& a, const bxGdiViewport& b )
	{
		return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;

	}
}}///

union bxGdiVertexStreamBlock
{
    bxGdiVertexStreamBlock(): hash(0) {}
    bxGdiVertexStreamBlock( bx::gdi::EVertexSlot sl, bx::gdi::EDataType t, int ne, int nrm = 0 )
        : slot( sl ), dataType(t), typeNorm(nrm), numElements(ne) {}

    u16 hash;
    struct
    {
        u16 slot        : 7;
        u16 dataType    : 4;
        u16 typeNorm    : 1;
        u16 numElements : 4;
    };
};
struct bxGdiVertexStreamDesc
{
    enum { eMAX_BLOCKS = 5 };
    bxGdiVertexStreamBlock blocks[eMAX_BLOCKS];
    i16 count;

    bxGdiVertexStreamDesc();
    bxGdiVertexStreamDesc& addBlock( bx::gdi::EVertexSlot slot, bx::gdi::EDataType type, int numElements, int norm = 0 );
};



struct bxGdiFormat
{
    u8 type;
    u8 numElements : 6;
    u8 normalized : 1;
    u8 srgb : 1;

    bxGdiFormat( bx::gdi::EDataType t, u8 nElem, u8 norm = 0, u8 s = 0 )
        : type( t ), numElements( nElem ), normalized( norm ), srgb(s)
    {}

    bxGdiFormat()
        : type(0), numElements(0), normalized(0), srgb(0) 
    {}
};

namespace bx{
namespace gdi{
    inline int formatByteWidth( const bxGdiFormat fmt ) { return typeStride[fmt.type] * fmt.numElements; }
}}///


union bxGdiSamplerDesc
{
    bxGdiSamplerDesc()
        : filter( bx::gdi::eFILTER_TRILINEAR_ANISO )
        , addressU( bx::gdi::eADDRESS_WRAP )
        , addressV( bx::gdi::eADDRESS_WRAP )
        , addressT( bx::gdi::eADDRESS_WRAP )
        , depthCmpMode( bx::gdi::eDEPTH_CMP_NONE )
        , aniso( 16 )
    {}
    bxGdiSamplerDesc( bx::gdi::ESamplerFilter f, bx::gdi::EAddressMode u, bx::gdi::EAddressMode v, bx::gdi::EAddressMode t, bx::gdi::ESamplerDepthCompare cmp, u32 a )
        : filter( u8(f) )
        , addressU( u8(u) )
        , addressV( u8(v) )
        , addressT( u8(t) )
        , depthCmpMode( u8(cmp) )
        , aniso( u8(a) )
    {}
    bxGdiSamplerDesc( bx::gdi::ESamplerFilter f, bx::gdi::EAddressMode uvt = bx::gdi::eADDRESS_CLAMP, bx::gdi::ESamplerDepthCompare cmp = bx::gdi::eDEPTH_CMP_NONE, u32 a = 16 )
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

struct bxGdiHwStateDesc
{
    union Blend
    {
	    u32 key;
	    struct  
	    {
		    u64 enable         : 1;
		    u64 color_mask     : 4;
            u64 equation       : 4;
            u64 srcFactor      : 4;
            u64 dstFactor      : 4;
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

    bxGdiHwStateDesc()
	{
		blend.enable = 0;
		blend.color_mask = bx::gdi::eCOLOR_MASK_ALL;
		blend.srcFactorAlpha = bx::gdi::eBLEND_ONE;
		blend.dstFactorAlpha = bx::gdi::eBLEND_ZERO;
        blend.srcFactor = bx::gdi::eBLEND_ONE;
        blend.dstFactor = bx::gdi::eBLEND_ZERO;
        blend.equation = bx::gdi::eBLEND_ADD;

		depth.function = bx::gdi::eDEPTH_FUNC_LEQUAL;
		depth.test = 1;
		depth.write = 1;

		raster.cullMode = bx::gdi::eCULL_BACK;
		raster.fillMode = bx::gdi::eFILL_SOLID;
		raster.multisample = 1;
		raster.antialiasedLine = 1;
		raster.scissor = 0;
	}
	Blend blend;
	Depth depth;
	Raster raster;
};
inline bool operator == ( const bxGdiHwStateDesc&  a, const bxGdiHwStateDesc& b )
{
    return a.blend.key == b.blend.key &&
           a.depth.key == b.depth.key &&
           a.raster.key == b.raster.key;
}
inline bool operator != ( const bxGdiHwStateDesc&  a, const bxGdiHwStateDesc& b )
{
    return !(a == b);
}

namespace bx{
namespace gdi{

    struct VertexP
    {
        f32 pos[3];
    };
    struct VertexNUV
    {
        f32 nrm[3];
        f32 uv[2];
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


namespace bx {
namespace gdi {

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

typedef bxGdiVertexStreamBlock VertexBufferDesc;

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
    bxGdiFormat format = {};
};
struct BufferRW : ResourceRW
{
    u32 sizeInBytes = 0;
    u32 bind_flags = 0;
    bxGdiFormat format;
};

struct TextureRO : ResourceRO
{
    u16 width = 0;
    u16 height = 0;
    u16 depth = 0;
    bxGdiFormat format = {};
};

struct TextureRW : ResourceRW
{
    ID3D11RenderTargetView* viewRT = nullptr;

    u16 width = 0;
    u16 height = 0;
    u16 depth = 0;
    bxGdiFormat format = {};
};

struct TextureDepth : ResourceRW
{
    ID3D11DepthStencilView* viewDS = nullptr;

    u16 width = 0;
    u16 height = 0;
    u16 depth = 0;
    bxGdiFormat format = {};
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

    void* inputSignature;
    i16 stage = -1;
    u16 vertexInputMask = 0;
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

struct VertexLayout
{
    VertexBufferDesc descs[cMAX_VERTEX_BUFFERS] = {};
    u32 count = 0;
};

typedef bxGdiSamplerDesc SamplerDesc;
typedef bxGdiHwStateDesc HardwareStateDesc;
typedef bx::gfx::Viewport Viewport;
//
struct CommandQueue;



}}///


