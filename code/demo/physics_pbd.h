#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bxPhysics
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
        const Vector3 goalPos = com + R * restPos;
        const Vector3 dpos = goalPos - pos;
        result[0] = dpos * shapeStiffness;
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
