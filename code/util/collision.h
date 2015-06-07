#pragma once

#include "vectormath/vectormath.h"

namespace bxOverlap
{
    int triangle_aabb( const Vector3& aabbCenter, const Vector3& aabbExt, const Vector3 triPoints[3] );
    int triangle_aabb( const Vector3& minAABB, const Vector3& maxAABB, const Vector3* triPoints, const unsigned triIndices[3] );
}///