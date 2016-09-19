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
    typedef struct VertexBufferImpl* VertexBuffer;

    struct ShaderBinary
    {
        void* code = nullptr;
        size_t size = 0;
    };

    typedef bxGdiVertexStreamBlock VertexStreamDesc;

    struct PipelineDesc
    {
        bxGdiShader shaders[ bxGdi::eDRAW_STAGES_COUNT ] = {};

        u32 num_vertex_stream_descs = 0;
        const VertexStreamDesc* vertex_stream_descs = nullptr;

        bxGdiHwStateDesc hw_state_desc = {};
        bxGdi::ETopology topology = bxGdi::eTRIANGLES;
    };
    
    struct RenderPassDesc
    {
        u16 num_color_textures = 0;
        u16 mips = 1;
        u16 width = 0;
        u16 height = 0;
        
        const bxGdiFormat* color_texture_formats = nullptr;
        bxGdi::EDataType depth_texture_type = bxGdi::eTYPE_UNKNOWN;
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
        eRESOURCE_TYPE_TEXTURE,
        eRESOURCE_TYPE_SAMPLER,
        eRESOURCE_TYPE_UNIFORM,
        eRESOURCE_TYPE_BUFFER_RO,
        eRESOURCE_TYPE_BUFFER_RW,
        _eRESOURCE_TYPE_COUNT_,
    };
    struct ResourceBinding
    {
        EResourceType type = _eRESOURCE_TYPE_COUNT_;
        u8 stage_mask = 0;
        u16 slot = 0;

        ResourceBinding() {}
        ResourceBinding( EResourceType t, u8 sm, u8 sl )
            : type( t ), stage_mask( sm ), slot( sl ) {}
    };

    struct ResourceLayout
    {
        ResourceBinding* bindings = nullptr;
        u32 num_bindings = 0;
    };

    struct VertexBufferDesc
    {
        u32 num_streams = 0;
        u32 has_indices = 0;
        u32 num_vertices = 0;
        u32 num_indices = 0;
        const bxGdi::EDataType index_type = bxGdi::eTYPE_UNKNOWN;
        const VertexBufferDesc* streams_desc = nullptr;
    };


    Pipeline createPipeline( bxGdiDeviceBackend* dev, const PipelineDesc& desc, bxAllocator* allocator = nullptr );
    void destroyPipeline( Pipeline* pipeline, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );
    void bindPipeline( bxGdiContextBackend* ctx, Pipeline pipeline );

    RenderPass createRenderPass( bxGdiDeviceBackend* dev, const RenderPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );

    RenderSubPass createRenderSubPass( const RenderSubPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderSubPass( RenderSubPass* subPass, bxAllocator* allocator = nullptr );
    void bindRenderSubPass( bxGdiContextBackend* ctx, RenderSubPass subPass );

    ResourceDescriptor createResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator = nullptr );
    void destroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator = nullptr );
    bool setTexture   ( ResourceDescriptor rdesc, bxGdiTexture texture, bxGdi::EStage stage, u16 slot );
    bool setSampler   ( ResourceDescriptor rdesc, bxGdiSampler sampler, bxGdi::EStage stage, u16 slot );
    bool setCBuffer   ( ResourceDescriptor rdesc, bxGdiBuffer buffer  , bxGdi::EStage stage, u16 slot );
    bool setBufferRO  ( ResourceDescriptor rdesc, bxGdiBuffer buffer  , bxGdi::EStage stage, u16 slot );
    bool setBufferRW  ( ResourceDescriptor rdesc, bxGdiBuffer buffer  , bxGdi::EStage stage, u16 slot );
    void bindResources( bxGdiContextBackend* ctx, ResourceDescriptor rdesc );

    bool createVertexBuffer( VertexBuffer* vbuffer, bxGdiDeviceBackend* dev, const VertexBufferDesc& desc, bxAllocator* allocator = nullptr );
    void destroyVertexBuffer( VertexBuffer* vbuffer, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );
    

    struct RenderBucket
    {
        RenderPass _render_pass;
    };
}}///

