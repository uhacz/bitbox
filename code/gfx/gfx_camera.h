#pragma once

#include <util/vectormath/vectormath.h>
#include <gdi/gdi_backend_struct.h>

struct bxGfxCameraMatrix
{
    Matrix4 world;
    Matrix4 view;
    Matrix4 proj;
    Matrix4 viewProj;

    Vector3 worldEye() const { return world.getTranslation(); }
    Vector3 worldDir() const { return -world.getCol2().getXYZ(); }

    bxGfxCameraMatrix();
};

struct bxGfxCameraParams
{
    f32 hAperture;
    f32 vAperture;
    f32 focalLength;
    f32 zNear;
    f32 zFar;
    f32 orthoWidth;
    f32 orthoHeight;

    bxGfxCameraParams();

    float aspect() const;
    float fov() const;
};

namespace bxGfx
{
    Matrix4 cameraMatrix_projection    ( float aspect, float fov, float znear, float zfar, int rtWidth, int rtHeight );
	Matrix4 cameraMatrix_projection    ( const bxGfxCameraParams& params, int rtWidth, int rtHeight );
    Matrix4 cameraMatrix_ortho         ( const bxGfxCameraParams& params, int rtWidth, int rtHeight );
    Matrix4 cameraMatrix_ortho         ( float orthoWidth, float orthoHeight, float znear, float zfar, int rtWidth, int rtHeight );
	Matrix4 cameraMatrix_view          ( const Matrix4& world );
	bxGdiViewport cameraParams_viewport( const bxGfxCameraParams& params, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void  cameraMatrix_compute               ( bxGfxCameraMatrix* mtx, const bxGfxCameraParams& params, const Matrix4& world, int rtWidth, int rtHeight );

	floatInVec camera_depth        ( const Matrix4& cameraWorld, const Vector3& worldPosition );
	inline int camera_filmFit ( int width, int height ) { return ( width > height ) ? 1 : 2; }
}///


struct bxGfxCamera
{
    bxGfxCameraMatrix matrix;
    bxGfxCameraParams params;
};