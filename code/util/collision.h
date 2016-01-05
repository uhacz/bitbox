#pragma once

#include "type.h"
#include "vectormath/vectormath.h"

namespace bx
{
    int overlapTriangleAABB( const Vector3& aabbCenter, const Vector3& aabbExt, const Vector3 triPoints[3] );
    int overlapTriangleAABB( const Vector3& minAABB, const Vector3& maxAABB, const Vector3* triPoints, const unsigned triIndices[3] );

    inline int testPointInTriangle( const Vector3& P, const Vector3& A, const Vector3& B, const Vector3& C )
    {
        const Vector3 v0 = C - A;
        const Vector3 v1 = B - A;
        const Vector3 v2 = P - A;

        union
        {
            __m128 dot128;
            struct
            {
                f32 dot00;
                f32 dot01;
                f32 dot02;
                f32 dot11;
            };
        };
        f32 dot12;

        Soa::Vector3 a( v0, v0, v0, v1 );
        Soa::Vector3 b( v0, v1, v2, v1 );

        dot128 = dot( a, b );
        dot12 = dot( v1, v2 ).getAsFloat();
        // Compute barycentric coordinates
        const float invDenom = 1.f / ( dot00 * dot11 - dot01 * dot01 );
        const float u = ( dot11 * dot02 - dot01 * dot12 ) * invDenom;
        const float v = ( dot00 * dot12 - dot01 * dot02 ) * invDenom;

        // Check if point is in triangle
        return ( u >= 0.f ) && ( v >= 0.f ) && ( u + v < 1.f );
    }
}///