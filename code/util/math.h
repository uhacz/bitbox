#pragma once

#include "type.h"
#include "vectormath/vectormath.h"

#define BX_POLAR_DECOMPOSITION_DEFAULT_TOLERANCE 0.0001f
#define BX_POLAR_DECOMPOSITION_DEFAULT_MAX_ITERATIONS 16

unsigned int bxPolarDecomposition( const Matrix3& a, Matrix3& u, Matrix3& h, unsigned maxIterations = BX_POLAR_DECOMPOSITION_DEFAULT_MAX_ITERATIONS );
Vector4 toAxisAngle( const Quat& q );


namespace bx
{
    inline float triangleArea( const Vector3 a, const Vector3 b, const Vector3 c )
    {
        return length( cross( a - b, a - c ) ).getAsFloat() * 0.5f;
    }
    inline float signedTriangleVolume( const Vector3 a, const Vector3 b, const Vector3 c )
    {
        return dot( a, cross( b, c ) ).getAsFloat() / 6.f;
    }
}///