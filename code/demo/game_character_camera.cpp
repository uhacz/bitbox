#include "game.h"
#include <gfx/gfx.h>
#include <util/debug.h>
#include <util/signal_filter.h>

namespace bx
{
    void characterCameraFollow( bx::GfxCamera* camera, const Vector3& characterPos, const Vector3& characterUpVector, float deltaTime, int cameraMoved )
    {
        const Matrix4& cameraPose = bx::gfxCameraWorldMatrixGet( camera );
        const Matrix3 cameraRot = cameraPose.getUpper3x3();
        const Vector3 cameraPos = bx::gfxCameraEye( camera );
        //const Matrix4 characterPose = characterPoseGet( character );
        //const Vector3 characterPosition = characterPose.getTranslation();
        //const Vector3 playerUpVector = characterUpVectorGet( character );

        const Vector3 toPlayerVec = (characterPos - cameraPos);
        const Vector3 toPlayerDir = normalize( toPlayerVec );

        const float RC = 0.1f; // (cameraMoved) ? 0.1f : 0.1f;
        const Vector3 z = -toPlayerDir; // signalFilter_lowPass( -toPlayerDir, cameraRot.getCol2(), RC, deltaTime );
        const Vector3 x = normalize( cross( characterUpVector, z ) );
        const Vector3 y = normalize( cross( z, x ) );

        const Matrix3 lookAtRot( x, y, z );

        //const Quat lookAtQ( lookAtRot );
        //const Quat cameraQ( cameraRot );

        //const floatInVec diff = dot( lookAtQ, cameraQ );
        //const floatInVec alpha = smoothstepf4( zeroVec, oneVec, diff );
        //const Quat rot = normalize( slerp( alpha, lookAtQ, cameraQ ) );
        //const Quat rot = normalize( slerp( deltaTime, cameraQ, lookAtQ ) );
        //camera->matrix.world.setUpper3x3( Matrix3( rot ) );
        //camera->matrix.world.setUpper3x3( lookAtRot );


        const floatInVec referenceDistance( 10.f );
        const floatInVec cameraPosStiffness( 1.f * deltaTime );

        Vector3 dpos( 0.f );
        {
            const Vector3 toPlayerVecProjected = projectVectorOnPlane( toPlayerVec, Vector4( characterUpVector, zeroVec ) );
            const floatInVec toPlayerDist = length( toPlayerVecProjected );
            const floatInVec diff = toPlayerDist - referenceDistance;
            dpos += toPlayerVecProjected * diff * cameraPosStiffness;
        }

        {
            const floatInVec height = dot( characterUpVector, cameraPos - characterPos );
            const floatInVec diff = maxf4( zeroVec, height - referenceDistance );
            dpos -= characterUpVector * diff * deltaTime;
        }

        //camera->matrix.world.setTranslation( cameraPos + dpos );

        bx::gfxCameraWorldMatrixSet( camera, Matrix4( lookAtRot, cameraPos + dpos ) );

    }

    void CameraController::follow( bx::GfxCamera* camera, const Vector3& characterPos, const Vector3& characterUpVector, float deltaTime, int cameraMoved /*= 0 */ )
    {
        const float fixedDt = 1.f / 60.f;
        _dtAcc += deltaTime;

        while( _dtAcc >= fixedDt )
        {
            characterCameraFollow( camera, characterPos, characterUpVector, fixedDt, cameraMoved );
            _dtAcc -= fixedDt;
        }
    }


    
}///