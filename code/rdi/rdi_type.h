#pragma once

#include <util\type.h>

namespace bx { namespace rdi {

//////////////////////////////////////////////////////////////////////////low level
#define BX_RDI_NULL_HANDLE nullptr
typedef struct PipelineImpl* Pipeline;
typedef struct RenderTargetImpl* RenderTarget;
typedef struct RenderSubPassImpl* RenderSubPass;
typedef struct ResourceDescriptorImpl* ResourceDescriptor;
typedef struct RenderSourceImpl* RenderSource;

//////////////////////////////////////////////////////////////////////////high level
typedef struct SceneImpl* Scene;
struct MeshInstance { u32 i = 0; };
struct Material { u32 i = 0; };
}}///