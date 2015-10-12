#include "view_frustum.h"
#include "camera.h"

namespace bx{namespace gfx{

    void viewFrustumExtractCorners( Vector3 dst[8], const Matrix4& viewProjection )
    {
        const Matrix4 vpInv = inverse( viewProjection );

        const Vector3 leftUpperNear = Vector3( 0.f, 1.f, 0.f );
        const Vector3 leftUpperFar = Vector3( 0.f, 1.f, 1.f );
        const Vector3 leftLowerNear = Vector3( 0.f, 0.f, 0.f );
        const Vector3 leftLowerFar = Vector3( 0.f, 0.f, 1.f );

        const Vector3 rightLowerNear = Vector3( 1.f, 0.f, 0.f );
        const Vector3 rightLowerFar = Vector3( 1.f, 0.f, 1.f );
        const Vector3 rightUpperNear = Vector3( 1.f, 1.f, 0.f );
        const Vector3 rightUpperFar = Vector3( 1.f, 1.f, 1.f );

        dst[0] = cameraUnprojectNormalized( leftUpperNear, vpInv );
        dst[1] = cameraUnprojectNormalized( leftUpperFar, vpInv );
        dst[2] = cameraUnprojectNormalized( leftLowerNear, vpInv );
        dst[3] = cameraUnprojectNormalized( leftLowerFar, vpInv );

        dst[4] = cameraUnprojectNormalized( rightLowerNear, vpInv );
        dst[5] = cameraUnprojectNormalized( rightLowerFar, vpInv );
        dst[6] = cameraUnprojectNormalized( rightUpperNear, vpInv );
        dst[7] = cameraUnprojectNormalized( rightUpperFar, vpInv );
    }

    inline void toSoa( Vector4& xxxx, Vector4& yyyy, Vector4& zzzz, Vector4& wwww,
                       const Vector4& vec0, const Vector4& vec1, const Vector4& vec2, const Vector4& vec3 )
    {
        Vector4 tmp0, tmp1, tmp2, tmp3;
        tmp0 = Vector4( vec_mergeh( vec0.get128(), vec2.get128() ) );
        tmp1 = Vector4( vec_mergeh( vec1.get128(), vec3.get128() ) );
        tmp2 = Vector4( vec_mergel( vec0.get128(), vec2.get128() ) );
        tmp3 = Vector4( vec_mergel( vec1.get128(), vec3.get128() ) );
        xxxx = Vector4( vec_mergeh( tmp0.get128(), tmp1.get128() ) );
        yyyy = Vector4( vec_mergel( tmp0.get128(), tmp1.get128() ) );
        zzzz = Vector4( vec_mergeh( tmp2.get128(), tmp3.get128() ) );
        wwww = Vector4( vec_mergel( tmp2.get128(), tmp3.get128() ) );
    }

    inline void toAos( Vector4& vec0, Vector4& vec1, Vector4& vec2, Vector4& vec3,
                       const Vector4& xxxx, const Vector4& yyyy, const Vector4& zzzz, const Vector4& wwww )
    {
        vec0 = Vector4( xxxx.getX(), yyyy.getX(), zzzz.getX(), wwww.getX() );
        vec1 = Vector4( xxxx.getY(), yyyy.getY(), zzzz.getY(), wwww.getY() );
        vec2 = Vector4( xxxx.getZ(), yyyy.getZ(), zzzz.getZ(), wwww.getZ() );
        vec3 = Vector4( xxxx.getW(), yyyy.getW(), zzzz.getW(), wwww.getW() );
    }

    void viewFrustumExtractPlanes( Vector4 planes[6], const Matrix4& vp )
    {
        floatInVec lengthInv;

        // each plane must be normalized, normalization formula is quite self explanatory

        planes[0] = vp.getRow( 0 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[0].getXYZ() );
        planes[0] = Vector4( planes[0].getXYZ() * lengthInv, planes[0].getW() * lengthInv );

        planes[1] = -vp.getRow( 0 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[1].getXYZ() );
        planes[1] = Vector4( planes[1].getXYZ() * lengthInv, planes[1].getW() * lengthInv );

        planes[2] = vp.getRow( 1 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[2].getXYZ() );
        planes[2] = Vector4( planes[2].getXYZ() * lengthInv, planes[2].getW() * lengthInv );

        planes[3] = -vp.getRow( 1 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[3].getXYZ() );
        planes[3] = Vector4( planes[3].getXYZ() * lengthInv, planes[3].getW() * lengthInv );

        planes[4] = vp.getRow( 2 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[4].getXYZ() );
        planes[4] = Vector4( planes[4].getXYZ() * lengthInv, planes[4].getW() * lengthInv );

        planes[5] = -vp.getRow( 2 ) + vp.getRow( 3 );
        lengthInv = floatInVec( 1.0f ) / length( planes[5].getXYZ() );
        planes[5] = Vector4( planes[5].getXYZ() * lengthInv, planes[5].getW() * lengthInv );
    }

    ViewFrustum viewFrustumExtract( const Matrix4& vp )
    {
        // http://www.lighthouse3d.com/opengl/viewfrustum/index.php?clipspace
        ViewFrustum f;
        Vector4 planes[6];
        const Vector4 zero4( 0.0f );

        viewFrustumExtractPlanes( planes, vp );
        toSoa( f.xPlaneLRBT, f.yPlaneLRBT, f.zPlaneLRBT, f.wPlaneLRBT,
               planes[0], planes[1], planes[2], planes[3] );
        toSoa( f.xPlaneNFNF, f.yPlaneNFNF, f.zPlaneNFNF, f.wPlaneNFNF,
               planes[4], planes[5], planes[4], planes[5] );

        return f;
    }

    void viewFrustumComputeTileCorners( Vector3 corners[8], const Matrix4& viewProjInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize )
    {
        const float pxm = (float)( tileSize * tileX );
        const float pym = (float)( tileSize * tileY );
        const float pxp = (float)( tileSize * ( tileX + 1 ) );
        const float pyp = (float)( tileSize * ( tileY + 1 ) );

        const float winWidth = (float)tileSize*numTilesX;
        const float winHeight = (float)tileSize*numTilesY;

        const float winWidthInv = 1.f / winWidth;
        const float winHeightInv = 1.f / winHeight;

        const float x0 = pxm * winWidthInv;
        const float x1 = pxp * winWidthInv;

        const float y0 = pym * winHeightInv;
        const float y1 = pyp * winHeightInv;

        const Vector3 leftUpperNear = Vector3( x0, y1, 0.f );
        const Vector3 leftUpperFar = Vector3( x0, y1, 1.f );
        const Vector3 leftLowerNear = Vector3( x0, y0, 0.f );
        const Vector3 leftLowerFar = Vector3( x0, y0, 1.f );
        const Vector3 rightLowerNear = Vector3( x1, y0, 0.f );
        const Vector3 rightLowerFar = Vector3( x1, y0, 1.f );
        const Vector3 rightUpperNear = Vector3( x1, y1, 0.f );
        const Vector3 rightUpperFar = Vector3( x1, y1, 1.f );

        corners[0] = cameraUnprojectNormalized( leftUpperNear, viewProjInv );
        corners[1] = cameraUnprojectNormalized( leftUpperFar, viewProjInv );
        corners[2] = cameraUnprojectNormalized( leftLowerNear, viewProjInv );
        corners[3] = cameraUnprojectNormalized( leftLowerFar, viewProjInv );
        corners[4] = cameraUnprojectNormalized( rightLowerNear, viewProjInv );
        corners[5] = cameraUnprojectNormalized( rightLowerFar, viewProjInv );
        corners[6] = cameraUnprojectNormalized( rightUpperNear, viewProjInv );
        corners[7] = cameraUnprojectNormalized( rightUpperFar, viewProjInv );
    }

    ViewFrustumLRBT viewFrustumTile( const Matrix4& viewProjInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize )
    {
        Vector3 corners[8];
        viewFrustumComputeTileCorners( corners, viewProjInv, tileX, tileY, numTilesX, numTilesY, tileSize );

        const Vector4 L = makePlane( normalize( cross( corners[2] - corners[0], corners[1] - corners[0] ) ), corners[0] );
        const Vector4 R = makePlane( normalize( cross( corners[6] - corners[4], corners[7] - corners[6] ) ), corners[6] );
        const Vector4 B = makePlane( normalize( cross( corners[4] - corners[2], corners[3] - corners[2] ) ), corners[2] );
        const Vector4 T = makePlane( normalize( cross( corners[0] - corners[6], corners[1] - corners[0] ) ), corners[0] );

        const Vector4 N( 0.f );
        const Vector4 F( 0.f );

        ViewFrustumLRBT f;
        toSoa( f.xPlaneLRBT, f.yPlaneLRBT, f.zPlaneLRBT, f.wPlaneLRBT, L, R, B, T );
        return f;
    }

}}///