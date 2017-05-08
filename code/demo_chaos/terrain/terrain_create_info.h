#pragma once

#include <util/type.h>

namespace bx{ namespace terrain{

struct CreateInfo
{
    enum { NUM_LODS = 4 };

    // size of single quad
    f32 tile_side_length = 1.f;

    // min and max divisions of single quad
    u32 min_tesselation_level = 2;
    u32 max_tesselation_level = min_tesselation_level + NUM_LODS;
    f32 radius[NUM_LODS] = {};
};

}}//
