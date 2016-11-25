#include "renderer_camera.h"
#include <util/camera.h>

namespace bx{ namespace gfx{

float Camera::aspect() const
{
    return( abs( params.vAperture ) < 0.0001f ) ? params.hAperture : params.hAperture / params.vAperture;
}
float Camera::fov() const
{
    return 2.f * atan( ( 0.5f * params.hAperture ) / ( params.focalLength * 0.03937f ) );
}
Vector3 Camera::worldEye() const
{
    return world.getTranslation();
}
Vector3 Camera::worldDir() const
{
    return -world.getCol2().getXYZ();
}

Viewport computeViewport( const Camera& camera, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
{
    return computeViewport( camera.aspect(), dstWidth, dstHeight, srcWidth, srcHeight );
}

void computeMatrices( Camera* cam )
{
    const float fov = cam->fov();
    const float aspect = cam->aspect();

    cam->view = inverse( cam->world );
    //cam->proj = Matrix4::perspective( fov, aspect, cam->params.zNear, cam->params.zFar );
    cam->proj = cameraMatrixProjection( aspect, fov, cam->params.zNear, cam->params.zFar );
    cam->proj = cameraMatrixProjectionDx11( cam->proj );
    cam->view_proj = cam->proj * cam->view;
}

}}///