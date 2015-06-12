#include "game.h"
#include <gfx/gfx_camera.h>
#include "util/debug.h"

namespace bxGame
{
    static inline Quat shortestRotation( const Vector3& v0, const Vector3& v1 )
    {
        const float d = dot( v0, v1 ).getAsFloat();
        const Vector3 c = cross( v0, v1 );
        const SSEScalar s( v0.get128() );

        Quat q = d > -1.f ? Quat( c, 1.f + d )
            : fabs( s.x ) < 0.1f ? Quat( 0.0f, s.z, -s.y, 0.0f ) : Quat( s.y, -s.x, 0.0f, 0.0f );

        return normalize( q );
    }

    static float computeAngle( const Vector3& v0, const Vector3& v1 )
    {
        const float cosine = dot( v0, v1 ).getAsFloat();
        const float sine = length( cross( v0, v1 ) ).getAsFloat();

        return ::atan2( sine, cosine );
    }
    void characterCamera_follow( bxGfxCamera* camera, const Character* character, float deltaTime )
    {
        const Matrix4& cameraPose = camera->matrix.world;
        Matrix3 cameraRot = cameraPose.getUpper3x3();
        const Vector3 cameraPos = camera->matrix.worldEye();
        const Matrix4 characterPose = character_pose( character );
        const Vector3 characterPosition = characterPose.getTranslation();
        const Vector3 playerUpVector = character_upVector( character );
        
        
        
        const Vector3 toPlayerVec = ( characterPosition - cameraPos);
        const Vector3 toPlayerDir = normalize( toPlayerVec );
        
        const Vector3 z = -toPlayerDir;
        const Vector3 x = normalize( cross( playerUpVector, z ) );
        const Vector3 y = normalize( cross( z, x ) );

        const Matrix3 lookAtRot( x, y, z );
        const Quat lookAtQ( lookAtRot );
        const Quat cameraQ( cameraRot );

        const floatInVec diff = dot( lookAtQ, cameraQ );
        const floatInVec alpha = smoothstepf4( floatInVec( 0.5f ), oneVec, diff );

        const Quat rot = normalize( slerp( alpha, lookAtQ, cameraQ ) );

        camera->matrix.world.setUpper3x3( Matrix3( rot ) );
    }
}///