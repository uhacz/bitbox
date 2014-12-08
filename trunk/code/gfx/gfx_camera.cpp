#include "gfx_camera.h"
#include "gfx_debug_draw.h"

#include <math.h>
#include <util/debug.h>

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
#include <util/common.h>
#include <util/signal_filter.h>
namespace bxGfx
{
    void cameraUtil_updateInput(bxGfxCameraInputContext* cameraCtx, const bxInput* input, float mouseSensitivityInPix, float dt )
    {
        const int fwd   = bxInput_isKeyPressed( &input->kbd, 'W' );
        const int back  = bxInput_isKeyPressed( &input->kbd, 'S' );
        const int left  = bxInput_isKeyPressed( &input->kbd, 'A' );
        const int right = bxInput_isKeyPressed( &input->kbd, 'D' );
        const int up    = bxInput_isKeyPressed( &input->kbd, 'Q' );
        const int down  = bxInput_isKeyPressed( &input->kbd, 'Z' );

        //bxLogInfo( "%d", fwd );

        const int mouse_lbutton = input->mouse.currentState()->lbutton;
        const int mouse_mbutton = input->mouse.currentState()->mbutton;
        const int mouse_rbutton = input->mouse.currentState()->rbutton;

        const int mouse_dx = input->mouse.currentState()->dx;
        const int mouse_dy = input->mouse.currentState()->dy;
        const float mouse_dxF = (float)mouse_dx;
        const float mouse_dyF = (float)mouse_dy;

        const float leftInputY = (mouse_dyF+mouse_dxF) * mouse_rbutton; // -float( back ) + float( fwd );
        const float leftInputX = mouse_dxF * mouse_mbutton; //-float( left ) + float( right );
        const float upDown     = mouse_dyF * mouse_mbutton; //-float( down ) + float( up );

        const float rightInputX = mouse_dxF * mouse_lbutton; // / mouseSensitivityInPix;
        const float rightInputY = mouse_dyF * mouse_lbutton; // / mouseSensitivityInPix;

        const float rc = 0.05f;

        cameraCtx->leftInputX  = signalFilter_lowPass( leftInputX , cameraCtx->leftInputX , rc, dt );
        cameraCtx->leftInputY  = signalFilter_lowPass( leftInputY , cameraCtx->leftInputY , rc, dt );
        cameraCtx->rightInputX = signalFilter_lowPass( rightInputX, cameraCtx->rightInputX, rc, dt );
        cameraCtx->rightInputY = signalFilter_lowPass( rightInputY, cameraCtx->rightInputY, rc, dt );
        cameraCtx->upDown      = signalFilter_lowPass( upDown     , cameraCtx->upDown     , rc, dt );
    }

    Matrix4 cameraUtil_movement( const Matrix4& world, float leftInputX, float leftInputY, float rightInputX, float rightInputY, float upDown )
    {
        Vector3 localPosDispl( 0.f );
        localPosDispl += Vector3::zAxis() * leftInputY * 0.15f;
        localPosDispl += Vector3::xAxis() * leftInputX * 0.15f;
        localPosDispl -= Vector3::yAxis() * upDown     * 0.15f;

        
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
}///


namespace bxGfx
{
    void viewFrustum_extractCorners( Vector3 dst[8], const Matrix4& viewProjection )
    {
        const Matrix4 vpInv = inverse( viewProjection );

        const Vector3 leftUpperNear  = Vector3(0.f, 1.f, 0.f);
        const Vector3 leftUpperFar   = Vector3(0.f, 1.f, 1.f);
        const Vector3 leftLowerNear  = Vector3(0.f, 0.f, 0.f);
        const Vector3 leftLowerFar   = Vector3(0.f, 0.f, 1.f);
                                                         
        const Vector3 rightLowerNear = Vector3(1.f, 0.f, 0.f);
        const Vector3 rightLowerFar  = Vector3(1.f, 0.f, 1.f);
        const Vector3 rightUpperNear = Vector3(1.f, 1.f, 0.f);
        const Vector3 rightUpperFar  = Vector3(1.f, 1.f, 1.f);

        dst[0] = camera_unprojectNormalized( leftUpperNear, vpInv );
        dst[1] = camera_unprojectNormalized( leftUpperFar, vpInv );
        dst[2] = camera_unprojectNormalized( leftLowerNear, vpInv );
        dst[3] = camera_unprojectNormalized( leftLowerFar, vpInv );

        dst[4] = camera_unprojectNormalized( rightLowerNear, vpInv );
        dst[5] = camera_unprojectNormalized( rightLowerFar, vpInv );
        dst[6] = camera_unprojectNormalized( rightUpperNear, vpInv );
        dst[7] = camera_unprojectNormalized( rightUpperFar, vpInv );
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

    void viewFrustum_extractPlanes( Vector4 planes[6], const Matrix4& vp )
    {
        floatInVec lengthInv;

        // each plane must be normalized, normalization formula is quite self explanatory

        planes[0] = vp.getRow(0) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[0].getXYZ());
        planes[0] = Vector4( planes[0].getXYZ() * lengthInv, planes[0].getW() * lengthInv );

        planes[1] = -vp.getRow(0) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[1].getXYZ());
        planes[1] = Vector4( planes[1].getXYZ() * lengthInv, planes[1].getW() * lengthInv );

        planes[2] = vp.getRow(1) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[2].getXYZ());
        planes[2] = Vector4( planes[2].getXYZ() * lengthInv, planes[2].getW() * lengthInv );

        planes[3] = -vp.getRow(1) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[3].getXYZ());
        planes[3] = Vector4( planes[3].getXYZ() * lengthInv, planes[3].getW() * lengthInv );

        planes[4] = vp.getRow(2) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[4].getXYZ());
        planes[4] = Vector4( planes[4].getXYZ() * lengthInv, planes[4].getW() * lengthInv );

        planes[5] = -vp.getRow(2) + vp.getRow(3);
        lengthInv = floatInVec(1.0f) / length(planes[5].getXYZ());
        planes[5] = Vector4( planes[5].getXYZ() * lengthInv, planes[5].getW() * lengthInv );
    }

    bxGfxViewFrustum viewFrustum_extract( const Matrix4& vp )
    {
        // http://www.lighthouse3d.com/opengl/viewfrustum/index.php?clipspace
        bxGfxViewFrustum f;
        Vector4 planes[6];
        const Vector4 zero4(0.0f);

        viewFrustum_extractPlanes( planes, vp );
        toSoa( f.xPlaneLRBT, f.yPlaneLRBT, f.zPlaneLRBT, f.wPlaneLRBT,
            planes[0], planes[1], planes[2], planes[3] );
        toSoa( f.xPlaneNFNF, f.yPlaneNFNF, f.zPlaneNFNF, f.wPlaneNFNF,
            planes[4], planes[5], planes[4], planes[5] );

        return f;
    }

    void viewFrustum_computeTileCorners( Vector3 corners[8], const Matrix4& projInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize )
    {
        const float pxm = (float)(tileSize * tileX);
        const float pym = (float)(tileSize * tileY);
        const float pxp = (float)(tileSize * (tileX + 1));
        const float pyp = (float)(tileSize * (tileY + 1));

        const float winWidth = (float)tileSize*numTilesX;
        const float winHeight = (float)tileSize*numTilesY;

        const float winWidthInv = 1.f / winWidth;
        const float winHeightInv = 1.f / winHeight;

        const float x0 = pxm * winWidthInv;
        const float x1 = pxp * winWidthInv;

        const float y0 = pym * winHeightInv;
        const float y1 = pyp * winHeightInv;

        const Vector3 leftUpperNear  = Vector3(x0, y1, 0.f);
        const Vector3 leftUpperFar   = Vector3(x0, y1, 1.f);
        const Vector3 leftLowerNear  = Vector3(x0, y0, 0.f);
        const Vector3 leftLowerFar   = Vector3(x0, y0, 1.f);
        const Vector3 rightLowerNear = Vector3(x1, y0, 0.f);
        const Vector3 rightLowerFar  = Vector3(x1, y0, 1.f);
        const Vector3 rightUpperNear = Vector3(x1, y1, 0.f);
        const Vector3 rightUpperFar  = Vector3(x1, y1, 1.f);

        corners[0] = camera_unprojectNormalized( leftUpperNear , projInv );
        corners[1] = camera_unprojectNormalized( leftUpperFar  , projInv );
        corners[2] = camera_unprojectNormalized( leftLowerNear , projInv );
        corners[3] = camera_unprojectNormalized( leftLowerFar  , projInv );
        corners[4] = camera_unprojectNormalized( rightLowerNear, projInv );
        corners[5] = camera_unprojectNormalized( rightLowerFar , projInv );
        corners[6] = camera_unprojectNormalized( rightUpperNear, projInv );
        corners[7] = camera_unprojectNormalized( rightUpperFar , projInv );
    }

    bxGfxViewFrustumLRBT viewFrustum_tile( const Matrix4& projInv, int tileX, int tileY, int numTilesX, int numTilesY, int tileSize )
    {
        Vector3 corners[8];
        viewFrustum_computeTileCorners( corners, projInv, tileX, tileY, numTilesX, numTilesY, tileSize );

        const Vector4 L = makePlane( normalize( cross( corners[2] - corners[0], corners[1] - corners[0] ) ), corners[0] );
        const Vector4 R = makePlane( normalize( cross( corners[6] - corners[4], corners[7] - corners[6] ) ), corners[6] );
        const Vector4 B = makePlane( normalize( cross( corners[4] - corners[2], corners[3] - corners[2] ) ), corners[2] );
        const Vector4 T = makePlane( normalize( cross( corners[0] - corners[6], corners[1] - corners[0] ) ), corners[0] );

        //bxGfxDebugDraw::addLine( lerp( 0.5f, corners[0], corners[2] ), lerp( 0.5f, corners[0], corners[2] ) + L.getXYZ() * 0.015f, 0xFFFF00FF, true );
        //bxGfxDebugDraw::addLine( lerp( 0.5f, corners[4], corners[6] ), lerp( 0.5f, corners[4], corners[6] ) + R.getXYZ() * 0.015f, 0xFF0000FF, true );
        //bxGfxDebugDraw::addLine( lerp( 0.5f, corners[2], corners[4] ), lerp( 0.5f, corners[2], corners[4] ) + B.getXYZ() * 0.015f, 0x00FF00FF, true );
        //bxGfxDebugDraw::addLine( lerp( 0.5f, corners[0], corners[6] ), lerp( 0.5f, corners[0], corners[6] ) + T.getXYZ() * 0.015f, 0x00FFFFFF, true );

        const Vector4 N(0.f);
        const Vector4 F(0.f);

        bxGfxViewFrustumLRBT f;
        toSoa( f.xPlaneLRBT, f.yPlaneLRBT, f.zPlaneLRBT, f.wPlaneLRBT, L, R, B, T );
        return f;
        
        //const float pxm = (float)(tileSize * tileX);
        //const float pym = (float)(tileSize * tileY);
        //const float pxp = (float)(tileSize * (tileX + 1));
        //const float pyp = (float)(tileSize * (tileY + 1));

        //const float winWidth = (float)tileSize*numTilesX;
        //const float winHeight = (float)tileSize*numTilesY;

        //const float winWidthInv = 1.f / winWidth;
        //const float winHeightInv = 1.f / winHeight;

        //const float x0 = pxm * winWidthInv;
        //const float x1 = pxp * winWidthInv;
        //
        //const float y0 = pym * winHeightInv;
        //const float y1 = pyp * winHeightInv;

        //const Vector3 leftUpperFar  = Vector3(x0, y1, 1.f);
        //const Vector3 leftLowerFar  = Vector3(x0, y0, 1.f);
        //const Vector3 rightLowerFar = Vector3(x1, y0, 1.f);
        //const Vector3 rightUpperFar = Vector3(x1, y1, 1.f);

        //const Vector3 leftTop  = camera_unprojectNormalized( leftUpperFar, projInv );
        //const Vector3 leftBtm  = camera_unprojectNormalized( leftLowerFar, projInv );
        //const Vector3 rightBtm = camera_unprojectNormalized( rightLowerFar, projInv );
        //const Vector3 rightTop = camera_unprojectNormalized( rightUpperFar, projInv );

        //plaszczyzny trzeba obliczyc na podstawie wszystkich 8 plaszczyzn

        //const Vector4 L = makePlane( normalize( rightBtm - leftBtm ), leftBtm );
        //const Vector4 R = makePlane( normalize( leftBtm - rightBtm ), rightBtm );
        //const Vector4 B = makePlane( normalize( leftTop - leftBtm ), leftBtm );
        //const Vector4 T = makePlane( normalize( leftBtm - leftTop ), leftTop );

        ////bxGfxDebugDraw::addLine( lerp( 0.5f, leftTop, leftBtm ), lerp( 0.5f, leftTop, leftBtm ) + L.getXYZ() * 0.5f, 0x0000FFFF, true );
        ////bxGfxDebugDraw::addLine( lerp( 0.5f, rightTop, rightBtm ), lerp( 0.5f, rightTop, rightBtm ) + R.getXYZ() * 0.5f, 0x0000FFFF, true );
        ////bxGfxDebugDraw::addLine( lerp( 0.5f, leftBtm, rightBtm ), lerp( 0.5f, leftBtm, rightBtm ) + B.getXYZ() * 0.5f, 0x0000FFFF, true );
        ////bxGfxDebugDraw::addLine( lerp( 0.5f, leftTop, rightTop ), lerp( 0.5f, leftTop, rightTop ) + T.getXYZ() * 0.5f, 0x0000FFFF, true );

        //const Vector4 N(0.f);
        //const Vector4 F(0.f);

        //bxGfxViewFrustum f;
        //toSoa( f.xPlaneLRBT, f.yPlaneLRBT, f.zPlaneLRBT, f.wPlaneLRBT, L, R, T, B );
        //toSoa( f.xPlaneNFNF, f.yPlaneNFNF, f.zPlaneNFNF, f.wPlaneNFNF, N, F, N, F );
        //return f;
    }

    void viewFrustum_debugDraw(const Matrix4& viewProj, u32 colorRGBA)
    {
        Vector3 corners[8];
        viewFrustum_extractCorners( corners, viewProj );
        viewFrustum_debugDraw( corners, colorRGBA );
        
        //const Vector3 leftUpperNear = Vector3( 0.f, 1.f, 0.f );
        //const Vector3 leftUpperFar = Vector3( 0.f, 1.f, 1.f );
        //const Vector3 leftLowerNear = Vector3( 0.f, 0.f, 0.f );
        //const Vector3 leftLowerFar = Vector3( 0.f, 0.f, 1.f );

        //const Vector3 rightLowerNear = Vector3( 1.f, 0.f, 0.f );
        //const Vector3 rightLowerFar = Vector3( 1.f, 0.f, 1.f );
        //const Vector3 rightUpperNear = Vector3( 1.f, 1.f, 0.f );
        //const Vector3 rightUpperFar = Vector3( 1.f, 1.f, 1.f );

    }

    void viewFrustum_debugDraw(const Vector3 corners[4], u32 colorRGBA)
    {
        bxGfxDebugDraw::addLine( corners[0], corners[1], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[2], corners[3], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[4], corners[5], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[6], corners[7], colorRGBA, true );

        bxGfxDebugDraw::addLine( corners[0], corners[2], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[2], corners[4], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[4], corners[6], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[6], corners[0], colorRGBA, true );

        bxGfxDebugDraw::addLine( corners[1], corners[3], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[3], corners[5], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[5], corners[7], colorRGBA, true );
        bxGfxDebugDraw::addLine( corners[7], corners[1], colorRGBA, true );
    }
}///
