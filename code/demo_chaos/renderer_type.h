#pragma once

#include <util/type.h>
#include <rdi/rdi_type.h>

#ifndef BX_CPP
#define BX_CPP
#endif
#include <shaders/shaders/sys/binding_map.h>


namespace bx{ namespace gfx{
    //////////////////////////////////////////////////////////////////////////high level
    typedef struct SceneImpl* Scene;
    struct ActorID { u32 i = 0; };
    struct MaterialID { u32 i = 0; };

    inline bool IsValid( ActorID id ) { return id.i != 0;  }
    inline bool IsValid( MaterialID id ) { return id.i != 0; }
}}///
