#include "game.h"
#include <gfx/gfx_camera.h>
#include "util/debug.h"
#include "util/signal_filter.h"

namespace bxGame
{
    void characterCamera_follow( bxGfxCamera* camera, const Character1* character, float deltaTime, int cameraMoved )
    {
        const Matrix4& cameraPose = camera->matrix.world;
        const Matrix3 cameraRot = cameraPose.getUpper3x3();
        const Vector3 cameraPos = camera->matrix.worldEye();
        const Matrix4 characterPose = character1_pose( character );
        const Vector3 characterPosition = characterPose.getTranslation();
        const Vector3 playerUpVector = character1_upVector( character );
        
        const Vector3 toPlayerVec = ( characterPosition - cameraPos);
        const Vector3 toPlayerDir = normalize( toPlayerVec );
        
        const float RC = (cameraMoved) ? 0.01f : 0.1f;
        const Vector3 z = signalFilter_lowPass( -toPlayerDir, cameraRot.getCol2(), RC, deltaTime );
        const Vector3 x = normalize( cross( playerUpVector, z ) );
        const Vector3 y = normalize( cross( z, x ) );

        const Matrix3 lookAtRot( x, y, z );

        //const Quat lookAtQ( lookAtRot );
        //const Quat cameraQ( cameraRot );

        //const floatInVec diff = dot( lookAtQ, cameraQ );
        //const floatInVec alpha = smoothstepf4( zeroVec, oneVec, diff );
        //const Quat rot = normalize( slerp( alpha, lookAtQ, cameraQ ) );
        //const Quat rot = normalize( slerp( deltaTime, cameraQ, lookAtQ ) );
        //camera->matrix.world.setUpper3x3( Matrix3( rot ) );

        camera->matrix.world.setUpper3x3( lookAtRot );

        const floatInVec referenceDistance( 15.f );
        const floatInVec cameraPosStiffness( 10.f * deltaTime );

        Vector3 dpos( 0.f );
        {
            const Vector3 toPlayerVecProjected = projectVectorOnPlane( toPlayerVec, Vector4( playerUpVector, zeroVec ) );
            const floatInVec toPlayerDist = length( toPlayerVecProjected );
            const floatInVec diff = toPlayerDist - referenceDistance;
            dpos += toPlayerVecProjected * diff * cameraPosStiffness;
        }

        {
            const floatInVec height = dot( playerUpVector, cameraPos - characterPosition );
            const floatInVec diff = maxf4( zeroVec, height - referenceDistance );
            dpos -= playerUpVector * diff * deltaTime;
        }

        camera->matrix.world.setTranslation( cameraPos + dpos );

    }
}///