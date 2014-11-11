#include "gfx_camera.h"

bxGfxCameraMatrix::bxGfxCameraMatrix()
    : world( Matrix4::identity() )
    , view( Matrix4::identity() )
    , proj( Matrix4::identity() )
    , viewProj( Matrix4::identity() )
    , worldViewProj( Matrix4::identity() )
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

