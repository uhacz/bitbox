#include "camera.h"
#include "signal_filter.h"

namespace bx{ namespace gfx{


    Matrix4 cameraMatrixProjection( float aspect, float fov, float znear, float zfar )
    {
        return Matrix4::perspective( fov, aspect, znear, zfar );
    }

	Matrix4 cameraMatrixProjectionDx11( const Matrix4 & proj )
	{
		const Matrix4 sc = Matrix4::scale( Vector3( 1, 1, 0.5f ) );
		const Matrix4 tr = Matrix4::translation( Vector3( 0, 0, 1 ) );
		return sc * tr * proj;
	}

    Matrix4 cameraMatrixOrtho( float orthoWidth, float orthoHeight, float znear, float zfar, int rtWidth, int rtHeight )
    {
        const float orthoWidthHalf = orthoWidth * 0.5f;
        const float orthoHeightHalf = orthoHeight * 0.5f;

        return Matrix4::orthographic( -orthoWidthHalf, orthoWidthHalf, -orthoHeightHalf, orthoHeightHalf, znear, zfar );
    }

    Matrix4 cameraMatrixOrtho( float left, float right, float bottom, float top, float near, float far )
    {
        return Matrix4::orthographic( left, right, bottom, top, near, far );
    }

    floatInVec cameraDepth( const Matrix4& cameraWorld, const Vector3& worldPosition )
    {
        const Vector3 inCameraSpace = worldPosition - cameraWorld.getTranslation();
        return -dot( cameraWorld.getCol2().getXYZ(), inCameraSpace );
    }

    Viewport computeViewport( float aspectCamera, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
    {
        const int windowWidth = dstWidth;
        const int windowHeight = dstHeight;

        const float aspectRT = (float)srcWidth / (float)srcHeight;
        
        int imageWidth;
        int imageHeight;
        int offsetX = 0, offsetY = 0;


        if( aspectCamera > aspectRT )
        {
            imageWidth = windowWidth;
            imageHeight = (int)( windowWidth / aspectCamera + 0.0001f );
            offsetY = windowHeight - imageHeight;
            offsetY = offsetY / 2;
        }
        else
        {
            float aspect_window = (float)windowWidth / (float)windowHeight;
            if( aspect_window <= aspectRT )
            {
                imageWidth = windowWidth;
                imageHeight = (int)( windowWidth / aspectRT + 0.0001f );
                offsetY = windowHeight - imageHeight;
                offsetY = offsetY / 2;
            }
            else
            {
                imageWidth = (int)( windowHeight * aspectRT + 0.0001f );
                imageHeight = windowHeight;
                offsetX = windowWidth - imageWidth;
                offsetX = offsetX / 2;
            }
        }

        return Viewport( offsetX, offsetY, imageWidth, imageHeight );
    }

    Matrix4 cameraUtilMovement( const Matrix4& world, float leftInputX, float leftInputY, float rightInputX, float rightInputY, float upDown, float sensitivity )
    {
        Vector3 localPosDispl( 0.f );
        localPosDispl += Vector3::zAxis() * leftInputY * sensitivity;
        localPosDispl += Vector3::xAxis() * leftInputX * sensitivity;
        localPosDispl -= Vector3::yAxis() * upDown     * sensitivity;


        //bxLogInfo( "%f; %f", rightInputX, rightInputY );

        const floatInVec rotDx( rightInputX );
        const floatInVec rotDy( rightInputY );

        const Matrix3 wmatrixRot( world.getUpper3x3() );

        const Quat worldRotYdispl = Quat::rotationY( -rotDx );
        const Quat worldRotXdispl = Quat::rotation( -rotDy, wmatrixRot.getCol0() );
        const Quat worldRotDispl = worldRotYdispl * worldRotXdispl;

        const Quat curWorldRot( wmatrixRot );

        const Quat worldRot = normalize( worldRotDispl * curWorldRot );
        const Vector3 worldPos = mulAsVec4( world, localPosDispl );

        return Matrix4( worldRot, worldPos );
    }


    CameraInputContext::CameraInputContext() 
        : _leftInputX( 0.f ), _leftInputY( 0.f )
        , _rightInputX( 0.f ), _rightInputY( 0.f )
        , _upDown( 0.f )
    {}

    void CameraInputContext::updateInput( int mouseL, int mouseM, int mouseR, int mouseDx, int mouseDy, float mouseSensitivityInPix, float dt )
    {
        const float mouse_dxF = (float)mouseDx;
        const float mouse_dyF = (float)mouseDy;

        const float new_leftInputY = ( mouse_dyF + mouse_dxF ) * mouseR;
        const float new_leftInputX = mouse_dxF * mouseM;
        const float new_upDown     = mouse_dyF * mouseM;

        const float new_rightInputX = mouse_dxF * mouseL * mouseSensitivityInPix;
        const float new_rightInputY = mouse_dyF * mouseL * mouseSensitivityInPix;

        const float rc = 0.05f;
        _leftInputX  = signalFilter_lowPass( new_leftInputX, _leftInputX, rc, dt );
        _leftInputY  = signalFilter_lowPass( new_leftInputY, _leftInputY, rc, dt );
        _rightInputX = signalFilter_lowPass( new_rightInputX, _rightInputX, rc, dt );
        _rightInputY = signalFilter_lowPass( new_rightInputY, _rightInputY, rc, dt );
        _upDown      = signalFilter_lowPass( new_upDown, _upDown, rc, dt );
    }

    void CameraInputContext::updateInput( float analogX, float analogY, float dt )
    {
        const float new_leftInputY = 0.f;
        const float new_leftInputX = analogX;
        const float new_upDown = analogY;

        const float new_rightInputX = 0.f;
        const float new_rightInputY = 0.f;
        
        const float rc = 0.1f;
        _leftInputX = signalFilter_lowPass( new_leftInputX, _leftInputX, rc, dt );
        _leftInputY = signalFilter_lowPass( new_leftInputY, _leftInputY, rc, dt );
        _rightInputX = signalFilter_lowPass( new_rightInputX, _rightInputX, rc, dt );
        _rightInputY = signalFilter_lowPass( new_rightInputY, _rightInputY, rc, dt );
        _upDown = signalFilter_lowPass( new_upDown, _upDown, rc, dt );
    }

    bool CameraInputContext::anyMovement() const
    {
        const float sum =
            ::abs( _leftInputX ) +
            ::abs( _leftInputY ) +
            ::abs( _rightInputX ) +
            ::abs( _rightInputY ) +
            ::abs( _upDown );

        return sum > 0.01f;
    }

    Matrix4 CameraInputContext::computeMovement( const Matrix4& world, float sensitivity ) const
    {
        Vector3 localPosDispl( 0.f );
        localPosDispl += Vector3::zAxis() * _leftInputY * sensitivity;
        localPosDispl += Vector3::xAxis() * _leftInputX * sensitivity;
        localPosDispl -= Vector3::yAxis() * _upDown     * sensitivity;
        //bxLogInfo( "%f; %f", rightInputX, rightInputY );

        const floatInVec rotDx( _rightInputX );
        const floatInVec rotDy( _rightInputY );

        const Matrix3 wmatrixRot( world.getUpper3x3() );

        const Quat worldRotYdispl = Quat::rotationY( -rotDx );
        const Quat worldRotXdispl = Quat::rotation( -rotDy, wmatrixRot.getCol0() );
        const Quat worldRotDispl = worldRotYdispl * worldRotXdispl;

        const Quat curWorldRot( wmatrixRot );

        const Quat worldRot = normalize( worldRotDispl * curWorldRot );
        const Vector3 worldPos = mulAsVec4( world, localPosDispl );

        return Matrix4( worldRot, worldPos );
    }


}
}///