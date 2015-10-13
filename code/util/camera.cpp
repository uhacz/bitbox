#include "camera.h"

namespace bx{ namespace gfx{


    Matrix4 cameraMatrixProjection( float aspect, float fov, float znear, float zfar )
    {
        return Matrix4::perspective( fov, aspect, znear, zfar );
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

}
}///