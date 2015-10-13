#pragma once

#include <util/vectormath/vectormath.h>

namespace bx{ namespace gfx{

    struct ViewFrustumLRBT
    {
        Vector4 xPlaneLRBT, yPlaneLRBT, zPlaneLRBT, wPlaneLRBT;
    };

    struct ViewFrustum : public ViewFrustumLRBT
    {
        Vector4 xPlaneNFNF, yPlaneNFNF, zPlaneNFNF, wPlaneNFNF;
    };

    void viewFrustumExtractCorners( Vector3 frustumCorners[8], const Matrix4& viewProjection );
    void viewFrustumExtractPlanes( Vector4 planesLRBTNF[6], const Matrix4& viewProjection );
    ViewFrustum viewFrustumExtract( const Matrix4& viewProjection );
    void viewFrustumComputeTileCorners( Vector3 corners[8], const Matrix4& viewProjInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize );
    ViewFrustumLRBT viewFrustumTile( const Matrix4& viewProjInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize );

    inline int viewFrustumSphereIntersectLRBT( const ViewFrustumLRBT& frustum, const Vector4& sphere, const floatInVec& tolerance = floatInVec( FLT_EPSILON ) );

    inline int viewFrustumAABBIntersectLRTB( const ViewFrustum& frustum, const Vector3& minc, const Vector3& maxc, const floatInVec& tolerance = floatInVec( FLT_EPSILON ) );
    inline boolInVec viewFrustumAABBIntersect( const ViewFrustum& frustum, const Vector3& minc, const Vector3& maxc, const floatInVec& tolerance = floatInVec( FLT_EPSILON ) );

#include "view_frustum.inl"
}}///
