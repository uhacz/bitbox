#pragma once

#include <gdi/gdi_shader.h>
#include <gdi/gdi_backend.h>

namespace bx{
namespace gfx{

    typedef struct PipelineImpl* Pipeline;
    typedef struct RenderPassImpl* RenderPass;
    typedef struct RenderSubPassImpl* RenderSubPass;
    typedef struct ResourceDescriptorImpl* ResourceDescriptor;

    struct ShaderBinary
    {
        void* code = nullptr;
        size_t size = 0;
    };

    struct PipelineDesc
    {
        u32 num_stages = 0;
        u32 num_vertex_stream_descs = 0;
        
        const ShaderBinary* shaders_butecode = nullptr;
        const bxGdiVertexStreamDesc* vertex_stream_descs = nullptr;

        bxGdiHwStateDesc hw_state_desc = {};
        bxGdi::ETopology topology = bxGdi::eTRIANGLES;
    };
    
    struct RenderPassDesc
    {
        u16 num_color_textures = 0;
        u16 has_depth_texture = 0;
        u16 width = 0;
        u16 height = 0;
        const bxGdiFormat* color_texture_formats = nullptr;
        bxGdiFormat depth_texture_format = {}
    };
    
    struct RenderSubPassDest
    {
        u32 num_color_textures = 0;
        u32 has_depth_texture = 0;
        u8* color_texture_indices = nullptr;

        RenderPass render_pass;
        Pipeline pipeline;
    };

    struct ResourceBinding
    {
        u16 slot;
        u16 stage;
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
        ResourceBinding* buffers_rq_binding = nullptr;
    };



    bool createPipeline( Pipeline* pipeline, bxGdiDeviceBackend* dev, const PipelineDesc& desc );
    void destroyPipeline( Pipeline* pipeline, bxGdiDeviceBackend* dev );

    bool createRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev, const RenderPassDesc& desc );
    void destroyRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev );

    bool createRenderSubPass( RenderSubPass* subPass, const RenderSubPassDest& desc );
    void destroyRenderSubPass( RenderSubPass* subPass );

    bool createResourceDescriptor( ResourceDescriptor* rdesc, const ResourceLayout& layout );
    void destroyResourceDescriptor( ResourceDescriptor* rdesc );

}}///

