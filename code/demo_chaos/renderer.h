#pragma once

#include <gdi/gdi_shader.h>
#include <gdi/gdi_backend.h>

namespace bx{
namespace gfx{

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

    struct ResourceBinding
    {
        u16 slot = 0;
        u16 stage = 0;
    };

    struct ResourceLayout
    {
        u8 num_textures = 0;
        u8 num_samplers = 0;
        u8 num_cbuffers = 0;
        u8 num_buffers_ro = 0;
        u8 num_buffers_rw = 0;
        u8 __padding[3];

        ResourceBinding* textures_binding = nullptr;
        ResourceBinding* samplers_binding = nullptr;
        ResourceBinding* cbuffers_binding = nullptr;
        ResourceBinding* buffers_ro_binding = nullptr;
        ResourceBinding* buffers_rw_binding = nullptr;
    };

    struct VertexBufferDesc
    {
        u32 num_streams = 0;
        u32 has_indices = 0;
        const VertexBufferDesc* streams_desc = nullptr;
        const bxGdi::EDataType index_type = bxGdi::eTYPE_UNKNOWN;
    };


    Pipeline createPipeline( bxGdiDeviceBackend* dev, const PipelineDesc& desc, bxAllocator* allocator = nullptr );
    void destroyPipeline( Pipeline* pipeline, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );

    RenderPass createRenderPass( bxGdiDeviceBackend* dev, const RenderPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );

    bool createRenderSubPass( RenderSubPass* subPass, const RenderSubPassDesc& desc, bxAllocator* allocator = nullptr );
    void destroyRenderSubPass( RenderSubPass* subPass, bxAllocator* allocator = nullptr );

    bool createResourceDescriptor( ResourceDescriptor* rdesc, const ResourceLayout& layout, bxAllocator* allocator = nullptr );
    void destroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator = nullptr );
    bool setTexture( ResourceDescriptor rdesc, bxGdiTexture texture, u16 slot, bxGdi::EStage stage );
    bool setSampler( ResourceDescriptor rdesc, bxGdiSampler sampler, u16 slot, bxGdi::EStage stage );
    bool setCBuffer( ResourceDescriptor rdesc, bxGdiBuffer buffer, u16 slot, bxGdi::EStage stage );
    bool setBufferRO( ResourceDescriptor rdesc, bxGdiBuffer buffer, u16 slot, bxGdi::EStage stage );
    bool setBufferRW( ResourceDescriptor rdesc, bxGdiBuffer buffer, u16 slot, bxGdi::EStage stage );

    bool createVertexBuffer( VertexBuffer* vbuffer, bxGdiDeviceBackend* dev, const VertexBufferDesc& desc, bxAllocator* allocator = nullptr );
    void destroyVertexBuffer( VertexBuffer* vbuffer, bxGdiDeviceBackend* dev, bxAllocator* allocator = nullptr );

    


    struct RenderBucket
    {
        RenderPass _render_pass;
    };
}}///

