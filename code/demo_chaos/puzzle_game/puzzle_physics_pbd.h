#pragma once

#include <util/vectormath/vectormath.h>
#include <util/math.h>

namespace bx{ namespace puzzle{ namespace physics
{

// --- constraints

// collision constraints are generated from scratch in every frame. 
// Thus particle indices are absolute for simplicity sake.
struct PlaneCollisionC
{
    Vector4F plane;
    u32 i;
};
struct ParticleCollisionC
{
    u32 i0;
    u32 i1;
};
struct SDFCollisionC
{
    Vector3F n;
    f32 d;
    u32 i0;
    u32 i1;
};

// in all constraints below particle indices are relative to body
struct DistanceC
{
    u32 i0;
    u32 i1;
    f32 rl;
};

struct BendingC
{
    u32 i0;
    u32 i1;
    u32 i2;
    u32 i3;
    f32 ra;
};

struct ShapeMatchingC
{
    Vector3F rest_pos;
    f32 mass;
};

//////////////////////////////////////////////////////////////////////////
inline int SolveDistanceC( Vector3F* resultA, Vector3F* resultB, const Vector3F& posA, const Vector3F& posB, f32 massInvA, f32 massInvB, f32 restLength, f32 stiffness )
{
    float wSum = massInvA + massInvB;
    if( wSum < FLT_EPSILON )
        return 0;

    float wSumInv = 1.f / wSum;

    Vector3F n = posB - posA;
    float d = length( n );
    n = ( d > FLT_EPSILON ) ? n / d : n;

    const float diff = d - restLength;
    const Vector3F dpos = n * stiffness * diff * wSumInv;

    resultA[0] = dpos * massInvA;
    resultB[0] = -dpos * massInvB;

    return 1;
}
//////////////////////////////////////////////////////////////////////////
static inline Vector3F ComputeFrictionDeltaPos( const Vector3F& tangent, const Vector3F& normal, float depth_positive, float sFriction, float dFriction )
{
    const float tangent_len = length( tangent );
    Vector3F dpos_friction( 0.f );
    if( tangent_len < depth_positive * sFriction )
    {
        dpos_friction = tangent;
    }
    else
    {
        const float a = minOfPair( depth_positive * dFriction / tangent_len, 1.f );
        dpos_friction = tangent * a;
    }

    return dpos_friction;
}
//////////////////////////////////////////////////////////////////////////

inline void SoftBodyUpdatePose( Matrix3F* rotation, Vector3F* centerOfMass, const Vector3F* pos, const ShapeMatchingC* shapeMatchingC, int nPoints )
{
    Vector3F com( 0.f );
    f32 totalMass( 0.f );
    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        const f32 mass = shapeMatchingC[ipoint].mass;
        com += pos[ipoint] * mass;
        totalMass += mass;
    }
    com /= totalMass;

    Vector3F col0( FLT_EPSILON, 0.f, 0.f );
    Vector3F col1( 0.f, FLT_EPSILON * 2.f, 0.f );
    Vector3F col2( 0.f, 0.f, FLT_EPSILON * 4.f );
    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        const f32 mass = shapeMatchingC[ipoint].mass;
        const Vector3F& q = shapeMatchingC[ipoint].rest_pos;
        const Vector3F p = ( pos[ipoint] - com ) * mass;
        col0 += p * q.getX();
        col1 += p * q.getY();
        col2 += p * q.getZ();
    }
    Matrix3F Apq( col0, col1, col2 );
    Matrix3F R, S;
    PolarDecomposition( Apq, R, S );

    rotation[0] = R;
    centerOfMass[0] = com;
}
static inline void SolveShapeMatchingC( Vector3F* result, const Matrix3F& R, const Vector3F& com, const Vector3F& restPos, const Vector3F& pos, float shapeStiffness )
{
    const Vector3F goalPos = com + R * ( restPos );
    const Vector3F dpos = goalPos - pos;
    result[0] = dpos * shapeStiffness;
}

//////////////////////////////////////////////////////////////////////////
}}}//
