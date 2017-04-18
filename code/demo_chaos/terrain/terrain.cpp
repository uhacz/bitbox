#include "terrain.h"
#include <util\array.h>

namespace bx{ namespace terrain{ 

typedef array_t<Vector3F> Vector3FArray;
typedef array_t<float2_t> Float2FArray;

inline u32 GetFlatIndex( u32 x, u32 y, u32 width )
{
    return y * width + x;
}

struct Instance
{
    CreateInfo _info = {};
    u32 num_points_per_tile = 0;
    u32 num_tiles = 0;



};

}}//