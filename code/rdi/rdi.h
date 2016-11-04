#pragma once

#include "rdi_type.h"
#include "rdi_backend.h"
#include <util/tag.h>
#include "util/debug.h"
#include <initializer_list>

struct bxAllocator;
struct bxPolyShape;

namespace bx{ 
    class ResourceManager;

namespace rdi {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct ShaderFile;

struct PipelineDesc
{
    ShaderFile* shader_file = nullptr;
    const char* shader_pass_name = nullptr;
    HardwareStateDesc* hw_state_desc_override = nullptr;
    ETopology::Enum topology = ETopology::TRIANGLES;

    PipelineDesc& Shader( ShaderFile* f, const char* pass_name )
    {
        shader_file = f;
        shader_pass_name = pass_name;
        return *this;
    }
    PipelineDesc& HardwareState( HardwareStateDesc* hwdesc ) { hw_state_desc_override = hwdesc; return *this; }
    PipelineDesc& Topology( ETopology::Enum t ) { topology = t; return *this;  }
};
Pipeline CreatePipeline( const PipelineDesc& desc, bxAllocator* allocator = nullptr );
void DestroyPipeline( Pipeline* pipeline, bxAllocator* allocator = nullptr );
void BindPipeline( CommandQueue* cmdq, Pipeline pipeline );
ResourceDescriptor GetResourceDescriptor( Pipeline pipeline );
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
void ClearRenderTarget( CommandQueue* cmdq, RenderTarget rtarget, float r, float g, float b, float a, float d );
void ClearRenderTargetDepth( CommandQueue* cmdq, RenderTarget rtarget, float d );
void BindRenderTarget( CommandQueue* cmdq, RenderTarget renderTarget, const std::initializer_list<u8>& colorTextureIndices, bool useDepth );
void BindRenderTarget( CommandQueue* cmdq, RenderTarget renderTarget );
TextureRW GetTexture( RenderTarget rtarget, u32 index );
TextureDepth GetTextureDepth( RenderTarget rtarget );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace EBindingType
{
    enum Enum : u8
    {
        READ_ONLY,
        READ_WRITE,
        UNIFORM,
        SAMPLER,
        _COUNT_,
    };
};
struct ResourceBinding
{
    const char* name = nullptr;
    EBindingType::Enum type = EBindingType::_COUNT_;
    u8 slot = 0;
    u16 stage_mask = 0;

    ResourceBinding() {}
    ResourceBinding( const char* n, EBindingType::Enum t, u8 sm, u8 sl, u8 cnt )
        : name( n ), type( t ), slot( sl ), stage_mask( sm ) {}

    ResourceBinding( const char* n, EBindingType::Enum t ) { name = n;  type = t; }
    ResourceBinding& StageMask( u8 sm ) { stage_mask = sm; return *this; }
    ResourceBinding& Slot( u8 sl ) { slot = sl; return *this; }
};

struct ResourceLayout
{
    ResourceBinding* bindings = nullptr;
    u32 num_bindings = 0;
};
struct ResourceDescriptorMemoryRequirments
{
    u32 descriptor_size = 0;
    u32 data_size = 0;
    u32 Total() const { return descriptor_size + data_size; }
};

ResourceDescriptorMemoryRequirments CalculateResourceDescriptorMemoryRequirments( const ResourceLayout& layout );
ResourceDescriptor CreateResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator = nullptr );
void DestroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator = nullptr );
u32 GenerateResourceHashedName( const char* name );
bool SetResourceRO( ResourceDescriptor rdesc, const char* name, const ResourceRO* resource );
bool SetResourceRW( ResourceDescriptor rdesc, const char* name, const ResourceRW* resource );
bool SetConstantBuffer( ResourceDescriptor rdesc, const char* name, const ConstantBuffer* cbuffer );
bool SetSampler( ResourceDescriptor rdesc, const char* name, const Sampler* sampler );
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
    EDataType::Enum index_type = EDataType::UNKNOWN;

    const void* vertex_data[cMAX_VERTEX_BUFFERS] = {};
    const void* index_data = nullptr;

    const RenderSourceRange* draw_ranges = nullptr;

    RenderSourceDesc& Count( u32 nVertices, u32 nIndices = 0 )
    {
        num_vertices = nVertices;
        num_indices = nIndices;
        return *this;
    }
    
    RenderSourceDesc& VertexBuffer( VertexBufferDesc vbDesc, const void* initialData )
    {
        SYS_ASSERT( vertex_layout.count < cMAX_VERTEX_BUFFERS );
        u32 index = vertex_layout.count++;
        vertex_layout.descs[index] = vbDesc;
        vertex_data[index] = initialData;
        return *this;
    }
    RenderSourceDesc& IndexBuffer( EDataType::Enum dt, const void* initialData )
    {
        index_type = dt;
        index_data = initialData;
        return *this;
    }
};
RenderSource CreateRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator = nullptr );
void DestroyRenderSource( RenderSource* rsource, bxAllocator* allocator = nullptr );
void BindRenderSource( CommandQueue* cmdq, RenderSource renderSource );
void SubmitRenderSource( CommandQueue* cmdq, RenderSource renderSource, u32 rangeIndex = 0 );
void SubmitRenderSourceInstanced( CommandQueue* cmdq, RenderSource renderSource, u32 numInstances, u32 rangeIndex = 0 );

u32 GetNVertexBuffers( RenderSource rsource );
u32 GetNVertices( RenderSource rsource );
u32 GetNIndices( RenderSource rsource );
u32 GetNRanges( RenderSource rsource );
VertexBuffer GetVertexBuffer( RenderSource rsource, u32 index );
IndexBuffer GetIndexBuffer( RenderSource rsource );
RenderSourceRange GetRange( RenderSource rsource, u32 index );
RenderSource CreateRenderSourceFromPolyShape( const bxPolyShape& shape );


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
        u32 offset_resource_descriptor = 0;
        u32 size_bytecode_pixel = 0;
        u32 size_bytecode_vertex = 0;
        u32 size_resource_descriptor;
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

}}///


namespace bx{ namespace rdi{

    struct DrawCommand
    {
        Pipeline pipeline = BX_RDI_NULL_HANDLE;
        ResourceDescriptor resources = BX_RDI_NULL_HANDLE;
        RenderSource rsource = BX_RDI_NULL_HANDLE;
        u8 rsouce_range = 0;
    };
    struct UpdateConstantBufferCommand
    {
        Resource resource = {};
        void* data = nullptr;
        u32 size = 0;
    };

    ComandBuffer CreateCommandBuffer();
    void DestroyCommandBuffer( ComandBuffer cmdBuff );
    void ClearCommandBuffer( ComandBuffer cmdBuff );





}}///