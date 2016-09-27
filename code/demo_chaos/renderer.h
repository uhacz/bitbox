#pragma once

#include <gdi/gdi_shader.h>
#include <gdi/gdi_backend.h>

namespace bx{
namespace gfx{

#define BX_GFX_NULL_HANDLE nullptr
    typedef struct PipelineImpl* Pipeline;
    typedef struct RenderPassImpl* RenderPass;
    typedef struct RenderSubPassImpl* RenderSubPass;
    typedef struct ResourceDescriptorImpl* ResourceDescriptor;
    typedef struct RenderSourceImpl* RenderSource;

    struct ShaderResource
    {
    };


    struct VertexLayout
    {
        gdi::VertexBufferDesc descs[gdi::cMAX_VERTEX_BUFFERS] = {};
        u32 count = 0;
    };

    struct PipelineDesc
    {
        gdi::Shader shaders[ gdi::eDRAW_STAGES_COUNT ] = {};
        VertexLayout vertex_layout = {};
        gdi::HardwareStateDesc hw_state_desc = {};
        gdi::ETopology topology = gdi::eTRIANGLES;
    };
    
    struct RenderPassDesc
    {
        u16 num_color_textures = 0;
        u16 mips = 1;
        u16 width = 0;
        u16 height = 0;
        
        const bxGdiFormat* color_texture_formats = nullptr;
        gdi::EDataType depth_texture_type = gdi::eTYPE_UNKNOWN;
    };
    
    struct RenderSubPassDesc
    {
        u32 num_color_textures = 0;
        u32 has_depth_texture = 0;
        u8* color_texture_indices = nullptr;
        RenderPass render_pass;
    };

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
    };

    struct ResourceLayout
    {
        ResourceBinding* bindings = nullptr;
        u32 num_bindings = 0;
    };

    struct RenderSourceDesc
    {
        u32 num_streams = 0;
        u32 num_vertices = 0;
        u32 num_indices = 0;
        VertexLayout vertex_layout = {};
        const gdi::EDataType index_type = gdi::eTYPE_UNKNOWN;
        
        const void* vertex_data[gdi::cMAX_VERTEX_BUFFERS] = {};
        const void* index_data = nullptr;
    };

    Pipeline createPipeline( const PipelineDesc& desc, bxAllocator* allocator = nullptr );
    void destroyPipeline( Pipeline* pipeline, bxAllocator* allocator = nullptr );
    void bindPipeline( gdi::CommandQueue* cmdq, Pipeline pipeline );

    RenderPass createRenderPass( const RenderPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderPass( RenderPass* renderPass, bxAllocator* allocator = nullptr );

    RenderSubPass createRenderSubPass( const RenderSubPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderSubPass( RenderSubPass* subPass, bxAllocator* allocator = nullptr );
    void bindRenderSubPass( gdi::CommandQueue* cmdq, RenderSubPass subPass );

    ResourceDescriptor createResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator = nullptr );
    void destroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator = nullptr );
    bool setResourceRO( ResourceDescriptor rdesc, const gdi::ResourceRO* resource, u8 stageMask, u8 slot );
    bool setResourceRW( ResourceDescriptor rdesc, const gdi::ResourceRW* resource, u8 stageMask, u8 slot );
    bool setConstantBuffer( ResourceDescriptor rdesc, const gdi::ConstantBuffer cbuffer, u8 stageMask, u8 slot );
    bool setSampler( ResourceDescriptor rdesc, const gdi::Sampler sampler, gdi::EStage stage, u16 slot );
    void bindResources( gdi::CommandQueue* cmdq, ResourceDescriptor rdesc );

    RenderSource createRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderSource( RenderSource* rsource, bxAllocator* allocator = nullptr );

}}///
