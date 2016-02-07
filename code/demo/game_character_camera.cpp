#include "game.h"
#include <gfx/gfx.h>
#include <util/debug.h>
#include <util/signal_filter.h>
#include <util/common.h>

namespace bx
{
    void characterCameraFollow( bx::GfxCamera* camera, const Vector3& characterPos, const Vector3& characterUpVector, const Vector3& characterDeltaPos, float deltaTime, float cameraMoved )
    {
        const Matrix4& cameraPose = bx::gfxCameraWorldMatrixGet( camera );
        const Matrix3 cameraRot = cameraPose.getUpper3x3();
        const Vector3 cameraPos = bx::gfxCameraEye( camera ) + characterDeltaPos;
        
        const Vector3 toPlayerVec = (characterPos - cameraPos);
        

        const floatInVec referenceDistance( 5.f );
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

		const Vector3 toPlayerDir = normalize( characterPos - (cameraPos) );
		const Vector3 cameraDir = bx::gfxCameraDir( camera );
		//const float d = dot( toPlayerDir, cameraDir ).getAsFloat();

		Matrix3 lookAtRot = cameraPose.getUpper3x3();
		//if( d < 0.9f )
		{
			const Vector3 z = -toPlayerDir;
			const Vector3 x = normalize( cross( characterUpVector, z ) );
			const Vector3 y = normalize( cross( z, x ) );
			
			//const float alpha = lerp( cameraMoved, deltaTime, 1.f );
			//const Quat lookAtQsmooth = slerp( alpha, Quat( lookAtRot ), Quat( Matrix3( x, y, z ) ) );
			//lookAtRot = Matrix3( lookAtQsmooth );
			lookAtRot = Matrix3( x, y, z );
		}

		 // ;

        //camera->matrix.world.setTranslation( cameraPos + dpos );

        bx::gfxCameraWorldMatrixSet( camera, Matrix4( lookAtRot, cameraPos + dpos ) );

    }

    void CameraController::follow( bx::GfxCamera* camera, const Vector3& characterPos, const Vector3& characterUpVector, const Vector3& characterDeltaPos, float deltaTime, int cameraMoved /*= 0 */ )
    {
        const float fixedDt = 1.f / 60.f;
        _dtAcc += deltaTime;
		_cameraMoved = signalFilter_lowPass( (float)cameraMoved, _cameraMoved, 0.05f, deltaTime );
        while( _dtAcc >= fixedDt )
        {
            characterCameraFollow( camera, characterPos, characterUpVector, characterDeltaPos, fixedDt, _cameraMoved );
            _dtAcc -= fixedDt;
        }
    }


    
}///