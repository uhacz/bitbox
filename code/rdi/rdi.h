#pragma once

#include "rdi_type.h"
#include "rdi_backend.h"
#include <util/tag.h>
#include "util/debug.h"
#include <initializer_list>

struct bxAllocator;

namespace bx{ 
    class ResourceManager;

namespace rdi {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct ShaderFile
{
    static const u32 VERSION = BX_UTIL_MAKE_VERSION( 1, 0, 0 );
    
    struct Pass
    {
        u32 hashed_name = 0;
        HardwareStateDesc hw_state_desc = {};
        VertexLayout vertex_layout = {};
        u32 offset_bytecode_pixel = 0;
        u32 offset_bytecode_vertex = 0;
        u32 size_bytecode_pixel = 0;
        u32 size_bytecode_vertex = 0;
    };

    u32 tag = bxTag32( "SF01" );
    u32 version = VERSION;
    u32 num_passes = 0;
    Pass passes[1];
};

u32 ShaderFileNameHash( const char* name, u32 version );
ShaderFile* ShaderFileLoad( const char* filename, ResourceManager* resourceManager );
void ShaderFileUnload( ShaderFile** sfile, ResourceManager* resourceManager );
u32 ShaderFileFindPass( const ShaderFile* sfile, const char* passName );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct PipelineDesc
{
    ShaderFile* shader_file = nullptr;
    const char* shader_pass_name = nullptr;
    HardwareStateDesc hw_state_desc = {};
    ETopology::Enum topology = ETopology::TRIANGLES;

    PipelineDesc& Shader( ShaderFile* f, const char* pass_name )
    {
        shader_file = f;
        shader_pass_name = pass_name;
        return *this;
    }
    PipelineDesc& HardwareState( const HardwareStateDesc hwdesc ) { hw_state_desc = hwdesc; return *this; }
    PipelineDesc& Topology( ETopology::Enum t ) { topology = t; return *this;  }
};
Pipeline CreatePipeline( const PipelineDesc& desc, bxAllocator* allocator = nullptr );
void DestroyPipeline( Pipeline* pipeline, bxAllocator* allocator = nullptr );
void BindPipeline( CommandQueue* cmdq, Pipeline pipeline );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderTargetDesc
{
    u16 num_color_textures = 0;
    u16 mips = 1;
    u16 width = 0;
    u16 height = 0;

    Format color_texture_formats[ cMAX_RENDER_TARGETS ] = {};
    EDataType::Enum depth_texture_type = EDataType::UNKNOWN;

    RenderTargetDesc& Size( u16 w, u16 h, u16 m = 1 )
    {
        width = w; height = h; mips = m;
        return *this;
    }
    RenderTargetDesc& Texture( Format format )
    {
        SYS_ASSERT( num_color_textures < cMAX_RENDER_TARGETS );
        color_texture_formats[num_color_textures++] = format;
        return *this;
    }
    RenderTargetDesc& Depth( EDataType::Enum dataType ) { depth_texture_type = dataType; return *this; }
};

RenderTarget CreateRenderTarget( const RenderTargetDesc& desc, bxAllocator* allocator = nullptr );
void DestroyRenderTarget( RenderTarget* renderPass, bxAllocator* allocator = nullptr );
void BindRenderTarget( CommandQueue* cmdq, RenderTarget renderTarget, const std::initializer_list<u8>& colorTextureIndices, bool useDepth );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
enum EResourceType : u8
{
    eRESOURCE_TYPE_READ_ONLY,
    eRESOURCE_TYPE_READ_WRITE,
    eRESOURCE_TYPE_UNIFORM,
    eRESOURCE_TYPE_SAMPLER,
    _eRESOURCE_TYPE_COUNT_,
};
struct ResourceBinding
{
    EResourceType type = _eRESOURCE_TYPE_COUNT_;
    u8 stage_mask = 0;
    u8 first_slot = 0;
    u8 count = 0;

    ResourceBinding() {}
    ResourceBinding( EResourceType t, u8 sm, u8 sl, u8 cnt )
        : type( t ), stage_mask( sm ), first_slot( sl ), count( cnt ) {}

    ResourceBinding( EResourceType t ) { type = t; }
    ResourceBinding& StageMask( u8 sm ) { stage_mask = sm; return *this; }
    ResourceBinding& FirstSlotAndCount( u8 fs, u8 cnt ) { first_slot = fs; count = cnt; return *this; }
};

struct ResourceLayout
{
    ResourceBinding* bindings = nullptr;
    u32 num_bindings = 0;
};
ResourceDescriptor CreateResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator = nullptr );
void DestroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator = nullptr );
bool SetResourceRO( ResourceDescriptor rdesc, const ResourceRO* resource, u8 stageMask, u8 slot );
bool SetResourceRW( ResourceDescriptor rdesc, const ResourceRW* resource, u8 stageMask, u8 slot );
bool SetConstantBuffer( ResourceDescriptor rdesc, const ConstantBuffer cbuffer, u8 stageMask, u8 slot );
bool SetSampler( ResourceDescriptor rdesc, const Sampler sampler, u8 stageMask, u8 slot );
void BindResources( CommandQueue* cmdq, ResourceDescriptor rdesc );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderSourceRange
{
    u32 begin = 0;
    u32 count = 0;
};
struct RenderSourceDesc
{
    u32 num_vertices = 0;
    u32 num_indices = 0;
    u32 num_draw_ranges = 0;
    VertexLayout vertex_layout = {};
    const EDataType::Enum index_type = EDataType::UNKNOWN;

    const void* vertex_data[cMAX_VERTEX_BUFFERS] = {};
    const void* index_data = nullptr;

    const RenderSourceRange* draw_ranges = nullptr;
};
RenderSource CreateRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator = nullptr );
void DestroyRenderSource( RenderSource* rsource, bxAllocator* allocator = nullptr );
void BindRenderSource( CommandQueue* cmdq, RenderSource renderSource );

u32 GetNVertexBuffers( RenderSource rsource );
u32 GetNVertices( RenderSource rsource );
u32 GetNIndices( RenderSource rsource );
u32 GetNRanges( RenderSource rsource );
VertexBuffer GetVertexBuffer( RenderSource rsource, u32 index );
IndexBuffer GetIndexBuffer( RenderSource rsource );
RenderSourceRange GetRange( RenderSource rsource, u32 index );

}}///
