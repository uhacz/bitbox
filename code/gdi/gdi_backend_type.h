#pragma once

#include <util/type.h>

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

        eTYPE_COUNT,
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
    static const char* typeName[] =
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


    EDataType typeFromName( const char* name );
    EDataType findBaseType( const char* name );

    enum ECpuAccess
    {
        eCPU_READ = BIT_OFFSET(0),
        eCPU_WRITE = BIT_OFFSET(1),
    };
    enum EGpuAccess
    {
        eGPU_READ = BIT_OFFSET(1),
        eGPU_WRITE = BIT_OFFSET(2), 
        eGPU_READ_WRITE = eGPU_READ | eGPU_WRITE,
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
    static const u32 cMAX_RESOURCES_RO = 8;
    static const u32 cMAX_RESOURCES_RW = 6;
    static const u32 cMAX_SAMPLERS = 8;
    static const u32 cMAX_VERTEX_BUFFERS = 6;
    static const u32 cMAX_SHADER_MACRO = 32;

    struct ShaderReflection;
}///