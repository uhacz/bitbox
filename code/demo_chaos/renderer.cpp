#include "renderer.h"

namespace bx {
namespace gfx {
    namespace utils
    {
        inline bxAllocator* getAllocator( bxAllocator* defaultAllocator )
        {
            return ( defaultAllocator ) ? defaultAllocator : bxDefaultAllocator();
        }
    }

    struct PipelineImpl
    {
        bxGdiBlendState blend_state = {};
        bxGdiDepthState depth_state = {};
        bxGdiRasterState raster_state = {};
        bxGdiInputLayout input_layout{};
        bxGdi::ETopology topology = bxGdi::eTRIANGLES;
        bxGdiShader      shaders[bxGdi::eDRAW_STAGES_COUNT] = {};
    };

    struct RenderPassImpl
    {
        bxGdiTexture color_textures[bxGdi::cMAX_RENDER_TARGETS] = {};
        bxGdiTexture depth_texture;
        u16 num_color_textures = 0;
    };

    struct RenderSubPassImpl
    {
        u8* color_texture_indices = nullptr;
        u32 has_depth_texture = 0;

        RenderPass render_pass = nullptr;
        Pipeline pipeline = nullptr;
    };
    struct ResourceDescriptorImpl
    {
        bxGdiTexture* textures = nullptr;
        bxGdiSampler* samplers = nullptr;
        bxGdiBuffer* cbuffers = nullptr;
        bxGdiBuffer* buffers_ro = nullptr;
        bxGdiBuffer* buffers_rw = nullptr;

        ResourceLayout layout = {};
    };

    //
    Pipeline createPipeline( bxGdiDeviceBackend* dev, const PipelineDesc& desc, bxAllocator* allocator )
    {
        PipelineImpl* impl = (PipelineImpl*)BX_NEW( utils::getAllocator( allocator ), PipelineImpl );

        for( u32 i = 0; i < bxGdi::eDRAW_STAGES_COUNT; ++i )
        {
            impl->shaders[i] = desc.shaders[i];
        }

        impl->input_layout = dev->createInputLayout( desc.vertex_stream_descs, desc.num_vertex_stream_descs, desc.shaders[bxGdi::eSTAGE_VERTEX] );
        impl->blend_state = dev->createBlendState( desc.hw_state_desc.blend );
        impl->depth_state = dev->createDepthState( desc.hw_state_desc.depth );
        impl->raster_state = dev->createRasterState( desc.hw_state_desc.raster );
        impl->topology = desc.topology;

        return impl;
    }


    void destroyPipeline( Pipeline* pipeline, bxGdiDeviceBackend* dev, bxAllocator* allocator /*= nullptr */ )
    {
        if( !pipeline[0] )
            return;

        PipelineImpl* impl = pipeline[0];
        dev->releaseRasterState( &impl->raster_state );
        dev->releaseDepthState( &impl->depth_state );
        dev->releaseBlendState( &impl->blend_state );
        dev->releaseInputLayout( &impl->input_layout );

        for( u32 i = 0; i < bxGdi::eDRAW_STAGES_COUNT; ++i )
        {
            impl->shaders[i] = {};
        }

        BX_DELETE( utils::getAllocator(allocator), impl );

        pipeline[0] = nullptr;
    }

    RenderPass createRenderPass( bxGdiDeviceBackend* dev, const RenderPassDesc& desc, bxAllocator* allocator /*= nullptr */ )
    {
        RenderPassImpl* impl = BX_NEW( utils::getAllocator( allocator ), RenderPassImpl );
        for( u32 i = 0; i < desc.num_color_textures; ++i )
        {
            impl->color_textures[i] = dev->createTexture2D( desc.width, desc.height, desc.mips, desc.color_texture_formats[i], bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, nullptr );
        }
        impl->num_color_textures = desc.num_color_textures;

        if( desc.depth_texture_type != bxGdi::eTYPE_UNKNOWN )
        {
            impl->depth_texture = dev->createTexture2Ddepth( desc.width, desc.height, desc.mips, desc.depth_texture_type, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
        }

        return impl;
    }

    void destroyRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev, bxAllocator* allocator /*= nullptr */ )
    {
        if( renderPass[0] == nullptr )
            return;

        RenderPassImpl* impl = renderPass[0];

        if( impl->depth_texture.id )
        {
            dev->releaseTexture( &impl->depth_texture );
        }

        for( u32 i = 0; i < impl->num_color_textures; ++i )
        {
            dev->releaseTexture( &impl->color_textures[i] );
        }

        BX_DELETE( utils::getAllocator( allocator ), impl );
        renderPass[0] = nullptr;
    }

}
}///
