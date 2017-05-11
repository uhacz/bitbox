#pragma once

#include <util/type.h>

namespace bx{ namespace terrain{

namespace Const
{
    namespace ELod
    {
        enum Enum
        {
            COUNT = 4,
            LAST_INDEX = COUNT - 1,
        };
    }//
}//

struct CreateInfo
{
    // size of single quad
    f32 tile_side_length = 1.f;

    // min and max divisions of single quad
    u32 min_tesselation_level = 2;
    f32 radius[Const::ELod::COUNT] = {};
};

}}//
