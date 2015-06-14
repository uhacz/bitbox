#include "physics_pbd.h"
#include <util/math.h>

namespace bxPhysics
{
    void pbd_softBodyUpdatePose( Matrix3* rotation, Vector3* centerOfMass, const Vector3* pos, const Vector3* restPos, const f32* mass, int nPoints )
    {
        Vector3 com( 0.f );
        floatInVec totalMass( 0.f );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            const floatInVec mass( mass[ipoint] );
            com += pos[ipoint] * mass;
            totalMass += mass;
        }
        com /= totalMass;

        Vector3 col0( FLT_EPSILON, 0.f, 0.f );
        Vector3 col1( 0.f, FLT_EPSILON * 2.f, 0.f );
        Vector3 col2( 0.f, 0.f, FLT_EPSILON * 4.f );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            const floatInVec mass( mass[ipoint] );
            const Vector3& q = restPos[ipoint];
            const Vector3 p = (pos[ipoint] - com) * mass;
            col0 += p * q.getX();
            col1 += p * q.getY();
            col2 += p * q.getZ();
        }
        Matrix3 Apq( col0, col1, col2 );
        Matrix3 R, S;
        bxPolarDecomposition( Apq, R, S );

        rotation[0] = R;
        centerOfMass[0] = com;
    }
}///