#pragma once

#include <util\type.h>
#include <util\containers.h>
#include <util\vectormath\vectormath.h>

namespace bx{ namespace flood{

typedef Vector3F Vec3;
typedef QuatF    Quat4;

typedef array_t<Vec3>  Vec3Array;
typedef array_t<Quat4> QuatArray;
typedef array_t<f32>   FloatArray;
typedef array_t<u32>   UintArray;

struct NeighbourIndices
{
    const u32* data = nullptr;
    u32 size = 0;

    bool Ok() const { return data && size; }
};

}}//
