#include "gfx_camera.h"
#include <math.h>

bxGfxCameraMatrix::bxGfxCameraMatrix()
    : world( Matrix4::identity() )
    , view( Matrix4::identity() )
    , proj( Matrix4::identity() )
    , viewProj( Matrix4::identity() )
{}

bxGfxCameraParams::bxGfxCameraParams()
    : hAperture( 1.8f )
	, vAperture( 1.f )
	, focalLength( 50.f )
	, zNear( 0.1f )
	, zFar( 1000.f )
	, orthoWidth( 10.f )
    , orthoHeight( 10.f )
{}

float bxGfxCameraParams::aspect() const
{
    return ( abs(vAperture) < 0.0001f ) ? hAperture : hAperture / vAperture;    
}
float bxGfxCameraParams::fov() const
{
    return 2.f * atan( ( 0.5f * hAperture ) / ( focalLength * 0.03937f ) ); 
}

namespace bxGfx
{
    Matrix4 cameraMatrix_projection( float aspect, float fov, float znear, float zfar, int rtWidth, int rtHeight )
    {
        float l, r, t, b;

	    const float aspectInv = 1.f / aspect;
	    const float rtAspect = (float)rtWidth / (float)rtHeight;
	    const float rtAspectInv = 1.f / rtAspect;

	    if( camera_filmFit( rtWidth, rtHeight ) == 1 )
	    {
		    if( rtAspect < aspect )
		    {
			    l =-znear * fov;
			    r = znear * fov;
			    b =-znear * fov * aspectInv;
			    t = znear * fov * aspectInv;
		    }	
		    else
		    {
			    l =-znear * fov;
			    r = znear * fov;
			    b =-znear * fov * rtAspectInv;
			    t = znear * fov * rtAspectInv;
		    }
	    }
	    else
	    {
		    if( rtAspect > aspect )
		    {
			    l =-znear * fov * aspect;
			    r = znear * fov * aspect;
			    b =-znear * fov;
			    t = znear * fov;
		    }
		    else
		    {
			    l =-znear* fov * rtAspect;
			    r = znear* fov * rtAspect;
			    b =-znear* fov;
			    t = znear* fov;
		    }
	    }
        // directx has different z mapping !!! (to range <0,1>)
        //
	    const Matrix4 proj = Matrix4::frustum( l, r, b, t, znear, zfar );
	    const Matrix4 sc = Matrix4::scale( Vector3(1,1,0.5f) );
	    const Matrix4 tr = Matrix4::translation( Vector3(0,0,1) );
	    const Matrix4 newProj = sc * tr * proj;
	    return newProj;
    }
	Matrix4 cameraMatrix_projection( const bxGfxCameraParams& params, int rtWidth, int rtHeight )
    {
        const float aspectCamera = params.aspect();
        const float fov = params.fov();
	    return cameraMatrix_projection( aspectCamera, fov, params.zNear, params.zFar, rtWidth, rtHeight );
    }
    Matrix4 cameraMatrix_ortho( const bxGfxCameraParams& params, int rtWidth, int rtHeight )
    {
        return cameraMatrix_ortho( params.orthoWidth, params.orthoHeight, params.zNear, params.zFar, rtWidth, rtHeight );
    }
    Matrix4 cameraMatrix_ortho( float orthoWidth, float orthoHeight, float znear, float zfar, int rtWidth, int rtHeight )
    {
        Matrix4 proj_matrix = Matrix4::identity();

        const float rtAspect = (float)rtWidth / (float)rtHeight;
        const float rtAspectInv = 1.f / rtAspect;
        const float othoWidthHalf  = orthoWidth * 0.5f;
        const float othoHeightHalf = orthoHeight * 0.5f;

        if( camera_filmFit( rtWidth, rtHeight ) == 1 )
        {
            if( rtAspect < 1.0f )
            {
                proj_matrix = Matrix4::orthographic( -othoWidthHalf, othoWidthHalf,-othoHeightHalf, othoHeightHalf, znear, zfar );
            }
            else
            {
                const float h = othoHeightHalf * rtAspectInv;
                proj_matrix = Matrix4::orthographic( -othoWidthHalf, othoWidthHalf, -h, h, znear, zfar );
            }
        }
        else
        {
            if( rtAspect > 1.0f )
            {
                proj_matrix = Matrix4::orthographic( -othoWidthHalf, othoWidthHalf, -othoHeightHalf, othoHeightHalf, znear, zfar );
            }
            else
            {
                const float h = othoWidthHalf * rtAspect;
                proj_matrix = Matrix4::orthographic( -h, h, -othoHeightHalf, othoHeightHalf, znear, zfar );
            }
        }

        // directx has different z mapping !!! (to range <0,1>)
        //
        const Matrix4 sc = Matrix4::scale( Vector3(1,1,0.5f) );
        const Matrix4 tr = Matrix4::translation( Vector3(0,0,1) );
        const Matrix4 newProj = sc * tr * proj_matrix;
        return newProj;
    }
	Matrix4 cameraMatrix_view( const Matrix4& world )
    {
        return inverse( world );
    }
	bxGdiViewport cameraParams_viewport( const bxGfxCameraParams& params, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
    {
    	const int windowWidth  = dstWidth;
	    const int windowHeight = dstHeight;

	    const float aspectRT = (float)srcWidth/(float)srcHeight;
        const float aspectCamera = params.aspect();

	    int imageWidth;
	    int imageHeight;
	    int offsetX = 0, offsetY = 0;
	
	
	    if ( aspectCamera > aspectRT )
	    {
		    imageWidth = windowWidth;
		    imageHeight = (int)( windowWidth / aspectCamera + 0.0001f );
		    offsetY = windowHeight - imageHeight;
		    offsetY = offsetY / 2;
	    }
	    else
	    {
		    float aspect_window = (float)windowWidth / (float)windowHeight;
		    if ( aspect_window <= aspectRT )
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

        return bxGdiViewport( offsetX, offsetY, imageWidth, imageHeight );
    }
    void cameraMatrix_compute( bxGfxCameraMatrix* mtx, const bxGfxCameraParams& params, const Matrix4& world, int rtWidth, int rtHeight )
    {
        mtx->view = cameraMatrix_view( world );
	    mtx->proj = cameraMatrix_projection( params, rtWidth, rtHeight );
	    mtx->viewProj = mtx->proj * mtx->view;
        mtx->world = world;
    }

	floatInVec camera_depth( const Matrix4& cameraWorld, const Vector3& worldPosition )
    {
        const Vector3 inCameraSpace = worldPosition - cameraWorld.getTranslation();
	    return -dot( cameraWorld.getCol2().getXYZ(), inCameraSpace );
    }
}

#include <system/input.h>
namespace bxGfx
{
    void cameraUtil_updateInput(bxGfxCameraInputContext* cameraCtx, const bxInput* input)
    {
        const int fwd   = bxInput_isKeyPressed( &input->kbd, 'W' );
        const int back  = bxInput_isKeyPressed( &input->kbd, 'S' );
        const int left  = bxInput_isKeyPressed( &input->kbd, 'A' );
        const int right = bxInput_isKeyPressed( &input->kbd, 'D' );
        const int up    = bxInput_isKeyPressed( &input->kbd, 'Q' );
        const int down  = bxInput_isKeyPressed( &input->kbd, 'Z' );

        const int mouse_lbutton = input->mouse.currentState()->lbutton;
        const int mouse_dx = (mouse_lbutton) ? input->mouse.currentState()->dx : 0;
        const int mouse_dy = (mouse_lbutton) ? input->mouse.currentState()->dy : 0;

        const float leftInputY = -float( back ) + float( fwd );
        const float leftInputX = -float( left ) + float( right );
        const float upDown     = -float( down ) + float( up );




    }

    Matrix4 cameraUtil_movement( const Matrix4& world, float leftInputX, float leftInputY, float rightInputX, float rightInputY, float upDown )
    {
        Vector3 localPosDispl( 0.f );
        localPosDispl -= Vector3::zAxis() * leftInputY;
        localPosDispl += Vector3::xAxis() * leftInputX;
        localPosDispl += Vector3::yAxis() * upDown;

        
        const floatInVec rotDx( rightInputX );
        const floatInVec rotDy( rightInputY );

        const Matrix3 wmatrixRot( world.getUpper3x3() );

        const Quat worldRotYdispl = Quat::rotationY( -rotDx );
        const Quat worldRotXdispl = Quat::rotation( -rotDy, wmatrixRot.getCol0() );
        const Quat worldRotDispl = worldRotYdispl * worldRotXdispl;

        const Quat curr_world_rot( wmatrixRot );

        const Quat world_rot = normalize( worldRotDispl * curr_world_rot );
        const Vector3 world_pos = mulAsVec4( world, localPosDispl );

        return Matrix4( world_rot, world_pos );
    }
}
