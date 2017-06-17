#pragma once

#include <util\type.h>

namespace bx { namespace rdi {

    struct CommandQueue;
//////////////////////////////////////////////////////////////////////////low level
#define BX_RDI_NULL_HANDLE nullptr
typedef struct PipelineImpl* Pipeline;
typedef struct RenderTargetImpl* RenderTarget;
typedef struct ResourceDescriptorImpl* ResourceDescriptor;
typedef struct RenderSourceImpl* RenderSource;
typedef struct CommandBufferImpl* CommandBuffer;
using DrawCallback = void( *)( CommandQueue* cmdq, u32 flags, void* userData );

}}///