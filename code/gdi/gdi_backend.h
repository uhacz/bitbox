#pragma once

namespace bxGdi
{
    enum EStage
    {
        eSTAGE_VERTEX = 0,
        eSTAGE_PIXEL,
        eSTAGE_GEOMETRY,
        eSTAGE_HULL,
        eSTAGE_DOMAIN,
        eSTAGE_COMPUTE,
        eSTAGE_COUNT,
        eDRAW_STAGES_COUNT = eSTAGE_DOMAIN + 1,

        eALL_STAGES_MASK = 0xFF,

        eSTAGE_MASK_VERTEX  = BIT_OFFSET( eSTAGE_VERTEX ),
        eSTAGE_MASK_PIXEL   = BIT_OFFSET( eSTAGE_PIXEL    ),
        eSTAGE_MASK_GEOMETRY= BIT_OFFSET( eSTAGE_GEOMETRY ),
        eSTAGE_MASK_HULL    = BIT_OFFSET( eSTAGE_HULL     ),
        eSTAGE_MASK_DOMAIN  = BIT_OFFSET( eSTAGE_DOMAIN   ),
        eSTAGE_MASK_COMPUTE = BIT_OFFSET( eSTAGE_COMPUTE  ),
    };
    static const char* stageName[eSTAGE_COUNT] =
    {
        "vertex",
        "pixel",
        "geometry",
        "hull",
        "domain",
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
        eTYPE_BYTE = 0,
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
}///

struct bxGdi_Device
{};

struct bxGdi_Context
{};

struct bxGdi_Id
{
    uptr i;
};
struct bxGdi_VertexBuffer : public bxGdi_Id {};
struct bxGdi_IndexBuffer  : public bxGdi_Id {};
struct bxGdi_Buffer       : public bxGdi_Id {};
struct bxGdi_Shader       : public bxGdi_Id {};
struct bxGdi_Texture      : public bxGdi_Id {};

union bxGdi_VertexStreamBlock
{
    u16 hash;
    struct
    {
        u16 slot        : 8;
        u16 dataType    : 4;
        u16 numElements : 4;
    };
};
struct bxGdi_VertexStreamDesc
{
    enum { eMAX_BLOCKS = 6 };
    bxGdi_VertexStreamBlock blocks[eMAX_BLOCKS];
    i32 count;

    bxGdi_VertexStreamDesc();
    void addBlock( bxGdi::EVertexSlot slot, bxGdi::EDataType type, int numElements );
};
namespace bxGdi
{
    int      blockStride( const bxGdi_VertexStreamBlock& block ); // { return typeStride[block.dataType] * block.numElements; }
    int      streamStride(const bxGdi_VertexStreamDesc& vdesc );
    unsigned streamSlotMask( const bxGdi_VertexStreamDesc& vdesc );
    inline int streamEqual( const bxGdi_VertexStreamDesc& a, const bxGdi_VertexStreamDesc& b )
    {
        SYS_STATIC_ASSERT( bxGdi_VertexStreamDesc::eMAX_BLOCKS == 6 );
        return ( a.count == b.count ) 
            && ( a.blocks[0].hash == b.blocks[0].hash )
            && ( a.blocks[1].hash == b.blocks[1].hash )
            && ( a.blocks[2].hash == b.blocks[2].hash )
            && ( a.blocks[3].hash == b.blocks[3].hash )
            && ( a.blocks[4].hash == b.blocks[4].hash )
            && ( a.blocks[5].hash == b.blocks[5].hash );
    }
};



struct bxGdi_HwState
{};

struct bxGdi_Sampler
{};
struct bxGdi_Viewport
{};

namespace bxGdi
{
    int  startup( bxGdi_Device** dev, bxGdi_Context** ctx, uptr hWnd, int winWidth, int winHeight, int fullScreen );
    void shutdown( bxGdi_Device** dev, bxGdi_Context** ctx );

    ///////////////////////////////////////////////////////////////// 
    bxGdi_Id releaseResource( bxGdi_Id* id );
    bxGdi_VertexBuffer createVertexBuffer( bxGdi_Device* dev );
    bxGdi_IndexBuffer  createIndexBuffer( bxGdi_Device* dev );

    bxGdi_Buffer createBuffer( bxGdi_Device* dev );
    bxGdi_Shader createShader( bxGdi_Device* dev );
    bxGdi_Shader createShader( bxGdi_Device* dev );
    bxGdi_Texture createTexture1D( bxGdi_Device* dev );
    bxGdi_Texture createTexture2D( bxGdi_Device* dev );
    bxGdi_Texture createTexture3D( bxGdi_Device* dev );
    bxGdi_Texture createTextureCube( bxGdi_Device* dev );

    /////////////////////////////////////////////////////////////////
    void clearState                ( bxGdi_Context* ctx );
    void setViewport               ( bxGdi_Context* ctx, const bxGdi_Viewport& vp );
    void setVertexBuffers          ( bxGdi_Context* ctx, bxGdi_VertexBuffer* vbuffers, const bxGdi_VertexStreamDesc* descs, unsigned start, unsigned n );
    void setIndexBuffer            ( bxGdi_Context* ctx, bxGdi_IndexBuffer ibuffer, int data_type );
    void setShaderPrograms         ( bxGdi_Context* ctx, bxGdi_Shader* shaders, int n );
    void setVertexFormat           ( bxGdi_Context* ctx, const bxGdi_VertexStreamDesc* descs, int ndescs, bxGdi_Shader vertex_shader );
    void setCbuffers               ( bxGdi_Context* ctx, bxGdi_Buffer* cbuffers , unsigned startSlot, unsigned n, int stage );
    void setTextures               ( bxGdi_Context* ctx, bxGdi_Texture* textures, unsigned startSlot, unsigned n, int stage );
    void setSamplers               ( bxGdi_Context* ctx, bxGdi_Sampler* samplers, unsigned startSlot, unsigned n, int stage );
    void setHwState                ( bxGdi_Context* ctx, const bxGdi_HwState& hwstate );
    void setTopology               ( bxGdi_Context* ctx, int topology );

    void changeToMainFramebuffer   ( bxGdi_Context* ctx );
    void changeRenderTargets       ( bxGdi_Context* ctx, bxGdi_Texture* colorTex, unsigned nColor, bxGdi_Texture depthTex );
    void clearBuffers              ( bxGdi_Context* ctx, bxGdi_Texture* colorTex, unsigned nColor, bxGdi_Texture depthTex, float rgbad[5], int flag_color, int flag_depth );

    void draw                      ( bxGdi_Context* ctx, unsigned numVertices, unsigned startIndex );
    void drawIndexed               ( bxGdi_Context* ctx, unsigned numIndices , unsigned startIndex, unsigned baseVertex );
    void drawInstanced             ( bxGdi_Context* ctx, unsigned numVertices, unsigned startIndex, unsigned numInstances );
    void drawIndexedInstanced      ( bxGdi_Context* ctx, unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex );

    unsigned char* mapVertices     ( bxGdi_Context* ctx, bxGdi_VertexBuffer vbuffer, int offsetInBytes, int mapType );
    unsigned char* mapIndices      ( bxGdi_Context* ctx, bxGdi_IndexBuffer ibuffer, int offsetInBytes, int mapType );
    void unmapVertices             ( bxGdi_Context* ctx, bxGdi_VertexBuffer vbuffer );
    void unmapIndices              ( bxGdi_Context* ctx, bxGdi_IndexBuffer ibuffer );
    void updateCBuffer             ( bxGdi_Context* ctx, bxGdi_Buffer cbuffer, const void* data );
    void swap                      ( bxGdi_Context* ctx );
    void generateMipmaps           ( bxGdi_Context* ctx, bxGdi_Texture texture );
}///
