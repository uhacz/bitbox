#pragma once

#include "vectormath/vectormath.h"

namespace bxOverlap
{
    int triangle_aabb( const Vector3& aabbCenter, const Vector3& aabbExt, const Vector3 triPoints[3] );
}///