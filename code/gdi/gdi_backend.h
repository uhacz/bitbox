#pragma once

#include <util/type.h>
#include <util/debug.h>

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

struct bxGdiDevice
{};

struct bxGdiContext
{};

struct bxGdiID
{
    uptr i;
};
struct bxGdiVertexBuffer : public bxGdiID {};
struct bxGdiIndexBuffer  : public bxGdiID {};
struct bxGdiBuffer       : public bxGdiID {};
struct bxGdiShader       : public bxGdiID {};
struct bxGdiTexture      : public bxGdiID {};
struct bxGdiInputLayout  : public bxGdiID {};
struct bxGdiBlendState   : public bxGdiID {};
struct bxGdiDepthState   : public bxGdiID {};
struct bxGdiRasterState  : public bxGdiID {};
struct bxGdiSampler      : public bxGdiID {};
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
    enum { eMAX_BLOCKS = 6 };
    bxGdiVertexStreamBlock blocks[eMAX_BLOCKS];
    i32 count;

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
        SYS_STATIC_ASSERT( bxGdiVertexStreamDesc::eMAX_BLOCKS == 6 );
        return ( a.count == b.count ) 
            && ( a.blocks[0].hash == b.blocks[0].hash )
            && ( a.blocks[1].hash == b.blocks[1].hash )
            && ( a.blocks[2].hash == b.blocks[2].hash )
            && ( a.blocks[3].hash == b.blocks[3].hash )
            && ( a.blocks[4].hash == b.blocks[4].hash )
            && ( a.blocks[5].hash == b.blocks[5].hash );
    }
};

struct bxGdiShaderReflection;

namespace bxGdi
{
    int  startup( bxGdiDevice** dev, bxGdiContext** ctx, uptr hWnd, int winWidth, int winHeight, int fullScreen );
    void shutdown( bxGdiDevice** dev, bxGdiContext** ctx );

    ///////////////////////////////////////////////////////////////// 
    bxGdiID releaseResource( bxGdiID* id );
    bxGdiVertexBuffer createVertexBuffer( bxGdiDevice* dev, const bxGdiVertexStreamDesc& desc, u32 numElements, const void* data = 0 );
    bxGdiIndexBuffer  createIndexBuffer( bxGdiDevice* dev, int dataType, u32 numElements, const void* data = 0  );

    bxGdiBuffer createBuffer( bxGdiDevice* dev, u32 sizeInBytes, u32 bindFlags );
    bxGdiShader createShader( bxGdiDevice* dev, int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bxGdiShaderReflection* reflection = 0);
    bxGdiShader createShader( bxGdiDevice* dev, int stage, const void* codeBlob, size_t codeBlobSizee, bxGdiShaderReflection* reflection = 0  );
    bxGdiTexture createTexture( bxGdiDevice* dev, const void* dataBlob, size_t dataBlobSize );
    bxGdiTexture createTexture1D( bxGdiDevice* dev );
    bxGdiTexture createTexture2D( bxGdiDevice* dev );
    bxGdiTexture createTexture3D( bxGdiDevice* dev );
    bxGdiTexture createTextureCube( bxGdiDevice* dev );
    bxGdiInputLayout createInputLayout( bxGdiDevice* dev, const bxGdiVertexStreamDesc* descs, int ndescs, bxGdiShader vertex_shader );
    bxGdiBlendState  createBlendState( bxGdiDevice* dev );
    bxGdiDepthState  createDepthState( bxGdiDevice* dev );
    bxGdiRasterState createRasterState( bxGdiDevice* dev );

    /////////////////////////////////////////////////////////////////
    void clearState                ( bxGdiContext* ctx );
    void setViewport               ( bxGdiContext* ctx, const bxGdiViewport& vp );
    void setVertexBuffers          ( bxGdiContext* ctx, bxGdiVertexBuffer* vbuffers, const bxGdiVertexStreamDesc* descs, unsigned start, unsigned n );
    void setIndexBuffer            ( bxGdiContext* ctx, bxGdiIndexBuffer ibuffer, int data_type );
    void setShaderPrograms         ( bxGdiContext* ctx, bxGdiShader* shaders, int n );
    void setInputLayout            ( bxGdiContext* ctx, const bxGdiInputLayout ilay );

    void setCbuffers               ( bxGdiContext* ctx, bxGdiBuffer* cbuffers , unsigned startSlot, unsigned n, int stage );
    void setTextures               ( bxGdiContext* ctx, bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage );
    void setSamplers               ( bxGdiContext* ctx, bxGdiSampler* samplers, unsigned startSlot, unsigned n, int stage );

    void setDepthState             ( bxGdiContext* ctx, const bxGdiDepthState state );
    void setBlendState             ( bxGdiContext* ctx, const bxGdiBlendState state );
    void setRasterState            ( bxGdiContext* ctx, const bxGdiRasterState state );

    void setTopology               ( bxGdiContext* ctx, int topology );

    void changeToMainFramebuffer   ( bxGdiContext* ctx );
    void changeRenderTargets       ( bxGdiContext* ctx, bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex );
    void clearBuffers              ( bxGdiContext* ctx, bxGdiTexture* colorTex, unsigned nColor, bxGdiTexture depthTex, float rgbad[5], int flag_color, int flag_depth );

    void draw                      ( bxGdiContext* ctx, unsigned numVertices, unsigned startIndex );
    void drawIndexed               ( bxGdiContext* ctx, unsigned numIndices , unsigned startIndex, unsigned baseVertex );
    void drawInstanced             ( bxGdiContext* ctx, unsigned numVertices, unsigned startIndex, unsigned numInstances );
    void drawIndexedInstanced      ( bxGdiContext* ctx, unsigned numIndices , unsigned startIndex, unsigned numInstances, unsigned baseVertex );

    unsigned char* mapVertices     ( bxGdiContext* ctx, bxGdiVertexBuffer vbuffer, int offsetInBytes, int mapType );
    unsigned char* mapIndices      ( bxGdiContext* ctx, bxGdiIndexBuffer ibuffer, int offsetInBytes, int mapType );
    void unmapVertices             ( bxGdiContext* ctx, bxGdiVertexBuffer vbuffer );
    void unmapIndices              ( bxGdiContext* ctx, bxGdiIndexBuffer ibuffer );
    void updateCBuffer             ( bxGdiContext* ctx, bxGdiBuffer cbuffer, const void* data );
    void swap                      ( bxGdiContext* ctx );
    void generateMipmaps           ( bxGdiContext* ctx, bxGdiTexture texture );
}///
