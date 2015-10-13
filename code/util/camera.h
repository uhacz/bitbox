#pragma once

#include "viewport.h"
#include <util/vectormath/vectormath.h>

namespace bx{ namespace gfx{

    Matrix4 cameraMatrixProjection( float aspect, float fov, float znear, float zfar );
    Matrix4 cameraMatrixOrtho( float orthoWidth, float orthoHeight, float znear, float zfar, int rtWidth, int rtHeight );
    Matrix4 cameraMatrixOrtho( float left, float right, float bottom, float top, float near, float far );
    floatInVec cameraDepth( const Matrix4& cameraWorld, const Vector3& worldPosition );
    
    inline int cameraFilmFit( int width, int height ) { return ( width > height ) ? 1 : 2; }
    inline Vector3 cameraUnprojectNormalized( const Vector3& screenPos01, const Matrix4& inverseMatrix )
    {
        Vector4 hpos = Vector4( screenPos01, oneVec );
        hpos = mulPerElem( hpos, Vector4( 2.0f ) ) - Vector4( 1.f );
        Vector4 worldPos = inverseMatrix * hpos;
        worldPos /= worldPos.getW();
        return worldPos.getXYZ();
    }

}}///