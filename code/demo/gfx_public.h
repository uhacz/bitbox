#pragma once

#include <util/type.h>

namespace bx
{
    struct GfxContext;
    struct GfxCommandQueue;

    struct GfxMeshInstance;
    struct GfxCamera;
    struct GfxScene;
    struct GfxViewport
    {
        i16 x, y;
        u16 w, h;
        GfxViewport() {}
        GfxViewport( int xx, int yy, unsigned ww, unsigned hh )
            : x( xx ), y( yy ), w( ww ), h( hh ) {}
    };
}///