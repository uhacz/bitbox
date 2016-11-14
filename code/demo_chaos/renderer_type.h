#pragma once

#include <util/type.h>
#include <rdi/rdi_type.h>

namespace bx{ namespace gfx{
    //////////////////////////////////////////////////////////////////////////high level
    typedef struct SceneImpl* Scene;
    struct MeshID { u32 i = 0; };
    struct MaterialID { u32 i = 0; };

    inline bool IsValid( MeshID id ) { return id.i != 0;  }
    inline bool IsValid( MaterialID id ) { return id.i != 0; }
}}///
