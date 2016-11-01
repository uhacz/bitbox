#pragma once

#include <util/vectormath/vectormath.h>
#include <util/viewport.h>


namespace bx{
namespace gfx{

    struct Camera
    {
        Matrix4 world = Matrix4::identity();
        Matrix4 view = Matrix4::identity();
        Matrix4 proj = Matrix4::identity();
        Matrix4 view_proj = Matrix4::identity();
        struct Params
        {
            f32 hAperture = 1.8f;
            f32 vAperture = 1.f;
            f32 focalLength = 50.f;
            f32 zNear = 0.25f;
            f32 zFar = 250.f;
            f32 orthoWidth = 10.f;
            f32 orthoHeight = 10.f;
        } params;

        float aspect() const;
        float fov() const;
        Vector3 worldEye() const;
        Vector3 worldDir() const;
    };
    Viewport computeViewport( const Camera& camera, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
    void computeMatrices( Camera* camera );
}}///
