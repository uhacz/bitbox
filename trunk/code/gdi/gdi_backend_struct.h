#pragma once

#include "gdi_backend_type.h"

struct bxGdiViewport
{
    i16 x, y;
    u16 w, h;

    bxGdiViewport() {}
    bxGdiViewport( int xx, int yy, unsigned ww, unsigned hh )
        : x(xx), y(yy), w(ww), h(hh) {}
};

namespace bxGdi
{
	inline int equal( const bxGdiViewport& a, const bxGdiViewport& b )
	{
		return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;

	}
}

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

namespace bxGdi
{
    struct VertexP
    {
        f32 pos[3];
    };
    struct VertexNUV
    {
        f32 nrm[3];
        f32 uv[2];
    };
}///