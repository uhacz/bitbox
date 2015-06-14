#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bxPhysics
{
    void pbd_softBodyUpdatePose( Matrix3* rotation, Vector3* centerOfMass, const Vector3* pos, const Vector3* restPos, const f32* mass, int nPoints );

    inline void pbd_solveShapeMatchingConstraint( Vector3* result, const Matrix3& R, const Vector3& com, const Vector3& restPos, const Vector3& pos, float shapeStiffness )
    {
        const Vector3 goalPos = com + R * restPos;
        const Vector3 dpos = goalPos - pos;
        result[0] = pos + dpos * shapeStiffness;
    }

}///
