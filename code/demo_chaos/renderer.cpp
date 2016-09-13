#include "renderer.h"

namespace bx {
namespace gfx {

    struct PipelineImpl
    {
        bxGdiBlendState blend_state = {};
        bxGdiDepthState depth_state = {};
        bxGdiRasterState raster_state = {};
        bxGdiInputLayout input_layout{};
        bxGdi::ETopology topology = bxGdi::eTRIANGLES;
        bxGdiShader*     shaders = nullptr;
        u32              num_shaders = 0;
    };

    struct RenderPass
    {
        bxGdiTexture* textures = nullptr;
        u16 num_color_textures = 0;
        u16 depth_texture_index = UINT16_MAX;
    };

    struct RenderSubPass
    {
        u8* color_texture_indices = nullptr;
        u32 has_depth_texture = 0;

        RenderPass render_pass = nullptr;
        Pipeline pipeline = nullptr;
    };
    struct ResourceDescriptor
    {
        bxGdiTexture* textures = nullptr;
        bxGdiSampler* samplers = nullptr;
        bxGdiBuffer* cbuffers = nullptr;
        bxGdiBuffer* buffers_ro = nullptr;
        bxGdiBuffer* buffers_rw = nullptr;

        ResourceLayout layout = {};
    };
        
}}///
