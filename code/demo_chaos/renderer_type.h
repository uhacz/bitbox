#pragma once

namespace bx{ namespace gfx{

    //////////////////////////////////////////////////////////////////////////low level
#define BX_GFX_NULL_HANDLE nullptr
    typedef struct PipelineImpl* Pipeline;
    typedef struct RenderPassImpl* RenderPass;
    typedef struct RenderSubPassImpl* RenderSubPass;
    typedef struct ResourceDescriptorImpl* ResourceDescriptor;
    typedef struct RenderSourceImpl* RenderSource;

    //////////////////////////////////////////////////////////////////////////high level
    typedef struct SceneImpl* Scene;
    struct MeshInstance { u32 i = 0; };
    struct Material { u32 i = 0; };

}}///
