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

// --- http://matthias-mueller-fischer.ch/publications/stablePolarDecomp.pdf
inline void extractRotation( QuatF &q, const Matrix3F &A, unsigned maxIter )
{
    for( unsigned int iter = 0; iter < maxIter; iter++ )
    {
        Matrix3F R( q );
        
        const Vector3F a = cross( R.getCol0(), A.getCol0() );
        const Vector3F b = cross( R.getCol1(), A.getCol1() );
        const Vector3F c = cross( R.getCol2(), A.getCol2() );

        const float d0 = dot( R.getCol0(), A.getCol0() );
        const float d1 = dot( R.getCol1(), A.getCol1() );
        const float d2 = dot( R.getCol2(), A.getCol2() );
        const float demon = 1.f / ( d0+d1+d2 ) + 1.0e-9f;

        const Vector3F omega = ( a + b + c ) * demon;
        const float w = length( omega );
        if( w < 1.0e-9 )
            break;

        q = QuatF::rotation( w, ( 1.0f / w ) * omega ) * q;
        q = normalize(q);
        
    #if 0
        Vector3F omega = 
            ( R.col( 0 ).cross( A.col( 0 ) ) + R.col( 1 ).cross( A.col( 1 ) ) + R.col( 2 ).cross( A.col( 2 ) ) ) * ( 1.0 / fabs( R.col( 0 ).dot( A.col( 0 ) ) + R.col( 1 ).dot( A.col( 1 ) ) + R.col( 2 ).dot( A.col( 2 ) ) ) + 1.0e-9 );
        
        double w = omega.norm();
        if( w < 1.0e-9 )
            break;
        q = Quaterniond( AngleAxisd( w, ( 1.0 / w ) * omega ) ) *
            q;
        q.normalize();
    #endif
    }
}
inline void SoftBodyUpdatePose1( QuatF* rotation, Vector3F* centerOfMass, const Vector3F* pos, const ShapeMatchingC* shapeMatchingC, int nPoints )
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
    centerOfMass[0] = com;

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
    extractRotation( rotation[0], Apq, 16 );

}
static inline void SolveShapeMatchingC( Vector3F* result, const QuatF& R, const Vector3F& com, const Vector3F& restPos, const Vector3F& pos, float shapeStiffness )
{
    const Vector3F goalPos = com + fastRotate( R, restPos );
    const Vector3F dpos = goalPos - pos;
    result[0] = dpos * shapeStiffness;
}

//inline void SoftBodyUpdatePose( Matrix3F* rotation, Vector3F* centerOfMass, const Vector3F* pos, const ShapeMatchingC* shapeMatchingC, int nPoints )
//{
//    Vector3F com( 0.f );
//    f32 totalMass( 0.f );
//    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
//    {
//        const f32 mass = shapeMatchingC[ipoint].mass;
//        com += pos[ipoint] * mass;
//        totalMass += mass;
//    }
//    com /= totalMass;
//
//    Vector3F col0( FLT_EPSILON, 0.f, 0.f );
//    Vector3F col1( 0.f, FLT_EPSILON * 2.f, 0.f );
//    Vector3F col2( 0.f, 0.f, FLT_EPSILON * 4.f );
//    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
//    {
//        const f32 mass = shapeMatchingC[ipoint].mass;
//        const Vector3F& q = shapeMatchingC[ipoint].rest_pos;
//        const Vector3F p = ( pos[ipoint] - com ) * mass;
//        col0 += p * q.getX();
//        col1 += p * q.getY();
//        col2 += p * q.getZ();
//    }
//    Matrix3F Apq( col0, col1, col2 );
//    Matrix3F R, S;
//    PolarDecomposition( Apq, R, S );
//
//    rotation[0] = R;
//    centerOfMass[0] = com;
//}
//static inline void SolveShapeMatchingC( Vector3F* result, const Matrix3F& R, const Vector3F& com, const Vector3F& restPos, const Vector3F& pos, float shapeStiffness )
//{
//    const Vector3F goalPos = com + R * ( restPos );
//    const Vector3F dpos = goalPos - pos;
//    result[0] = dpos * shapeStiffness;
//}

//////////////////////////////////////////////////////////////////////////
}}}//
