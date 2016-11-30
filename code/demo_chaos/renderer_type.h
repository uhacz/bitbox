#pragma once

#include <util/type.h>
#include <rdi/rdi_type.h>

#include <shaders/shaders/sys/binding_map.h>


namespace bx{ namespace gfx{
    //////////////////////////////////////////////////////////////////////////high level
    typedef struct SceneImpl* Scene;
    struct ActorID { u32 i = 0; };
    struct MaterialHandle { u32 i = 0; };
    struct MeshHandle { u32 i; };

    inline bool IsValid( ActorID id ) { return id.i != 0;  }
    inline bool IsValid( MaterialHandle id ) { return id.i != 0; }
    
    inline bool IsValid( MeshHandle h ) { return h.i != 0; }
    //inline bool IsValid( MaterialHandle h ) { return h.i != 0; }
    //inline bool IsValid( TextureHandle1 h ) { return h.i != 0; }

}}///
