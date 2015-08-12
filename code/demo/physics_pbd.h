#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bxPhx
{
    ////
    ////
    void pbd_softBodyUpdatePose( Matrix3* rotation, Vector3* centerOfMass, const Vector3* pos, const Vector3* restPos, const f32* mass, int nPoints );

    ////
    ////
    

    ////
    ////
    inline void pbd_solveShapeMatchingConstraint( Vector3* result, const Matrix3& R, const Vector3& com, const Vector3& restPos, const Vector3& pos, float shapeStiffness )
    {
        const Vector3 goalPos = com + R * ( restPos );
        const Vector3 dpos = goalPos - pos;
        result[0] = dpos * shapeStiffness;
    }
    ////
    ////
    inline int pbd_solveDistanceConstraint( 
        Vector3* resultA, Vector3* resultB, 
        const Vector3& posA, const Vector3& posB, 
        float massInvA, float massInvB, 
        float restLength, float compressionStiffness, float stretchStiffness )
    {
        float wSum = massInvA + massInvB;
        if( wSum < FLT_EPSILON )
            return 0;

        float wSumInv = 1.f / wSum;


        Vector3 n = posB - posA;
        float d = length( n ).getAsFloat();
        n = ( d > FLT_EPSILON ) ? n / d: n;
        
        const float diff = d - restLength;
        const float stiffness = ( d < restLength ) ? compressionStiffness : stretchStiffness;
        const Vector3 dpos = n * stiffness * diff * wSumInv;

        resultA[0] = dpos * massInvA;
        resultB[0] =-dpos * massInvB;

        return 1;
    }
    ////
    ////
    inline int pbd_solveRepulsionConstraint(
        Vector3* resultA, Vector3* resultB,
        const Vector3& posA, const Vector3& posB,
        float massInvA, float massInvB,
        float restLength, float stiffness )
    {
        float wSum = massInvA + massInvB;
        if( wSum < FLT_EPSILON )
        {
            resultA[0] = Vector3( 0.f );
            resultB[0] = Vector3( 0.f );
            return 0;
        }
        float wSumInv = 1.f / wSum;


        Vector3 n = posB - posA;
        float d = length( n ).getAsFloat();
        if( d > restLength )
        {
            resultA[0] = Vector3( 0.f );
            resultB[0] = Vector3( 0.f );
            return 0;
        }
        n = ( d > FLT_EPSILON ) ? n / d : n;

        const float diff = d - restLength;
        const Vector3 dpos = n * stiffness * diff * wSumInv;

        resultA[0] = dpos * massInvA;
        resultB[0] = -dpos * massInvB;

        return 1;
    }
    ////
    ////
    inline void pbd_computeFriction( Vector3* result, const Vector3& pos0, const Vector3& pos1, const Vector3& normal, float sfriction, float dfriction )
    {
        const Vector3 v = pos1 - pos0;
        const Vector3 t = projectVectorOnPlane( v, Vector4( normal, zeroVec ) );

        const floatInVec tspeed = length( t );
        const floatInVec speed = length( v );
        const floatInVec sd = speed * floatInVec( sfriction );
        const floatInVec dd = speed * floatInVec( dfriction );
        const floatInVec tspeedInv = select( zeroVec, oneVec / tspeed, tspeed > fltEpsVec );
        const Vector3 tmp = -t * minf4( oneVec, dd * tspeedInv );
        result[0] = select( tmp, -t, tspeed < sd );
    }

}///
