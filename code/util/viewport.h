#pragma once

#include <util/type.h>

namespace bx{ namespace gfx{

    struct Viewport
    {
        i16 x, y;
        u16 w, h;
        Viewport() {}
        Viewport( int xx, int yy, unsigned ww, unsigned hh )
            : x( xx ), y( yy ), w( ww ), h( hh ) {}
    };
    Viewport computeViewport( float aspectCamera, int dstWidth, int dstHeight, int srcWidth, int srcHeight );

}}//