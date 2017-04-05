#pragma once

#include <util/vectormath/vectormath.h>

namespace bx{ namespace flood{

namespace pbd
{
    inline int SolveDistanceConstraint( 
        Vector3F result[2],
        const Vector3F& posA, const Vector3F& posB,
        float massInvA, float massInvB,
        float restLength, float compressionStiffness, float stretchStiffness )
    {
        const float wSum = massInvA + massInvB;
        if( wSum < FLT_EPSILON )
            return 0;

        const float wSumInv = 1.f / wSum;

        Vector3F n = posB - posA;
        const float d = length( n );
        n = ( d > FLT_EPSILON ) ? n / d : n;

        const float diff = d - restLength;
        const float stiffness = ( d < restLength ) ? compressionStiffness : stretchStiffness;
        const Vector3F dpos = n * stiffness * diff * wSumInv;

        result[0] = dpos * massInvA;
        result[1] = -dpos * massInvB;

        return 1;
    }

    inline int SolveDihedralConstraint( Vector3F result[4],
        const Vector3F& p0, const Vector3F& p1, const Vector3F& p2, const Vector3F& p3, 
        float w0, float w1, float w2, float w3,
        float restAngle, float stiffness )
    {
    }


}

}}

