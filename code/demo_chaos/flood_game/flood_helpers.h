#pragma once

#include <util\type.h>
#include <util\vectormath\vectormath.h>

namespace bx{ namespace flood{

typedef Vector3F Vec3;
typedef QuatF    Quat4;

struct NeighbourIndices
{
    const u32* data = nullptr;
    u32 size = 0;

    bool Ok() const { return data && size; }
};

}}//
