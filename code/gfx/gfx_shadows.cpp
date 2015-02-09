#include "gfx_shadows.h"
#include "gfx_camera.h"
#include "gfx_render_list.h"
#include "gfx_sort_list.h"
#include "gfx_debug_draw.h"
#include <gdi/gdi_shader.h>

#include <util/common.h>

void bxGfx::sortList_computeShadow( bxGfxSortList_Shadow* sList, const bxGfxRenderList& rList, const bxGfxShadows_Cascade* cascades, int nCascades, u8 renderMask /*= eRENDER_MASK_SHADOW */ )
{
    SYS_ASSERT( nCascades <= bxGfx::eSHADOW_NUM_CASCADES );

    bxGfxViewFrustum frustums[bxGfx::eSHADOW_NUM_CASCADES];
    Matrix4 cascadeWorldMatrices[bxGfx::eSHADOW_NUM_CASCADES];

    for( int i = 0; i < nCascades; ++i )
    {
        frustums[i] = bxGfx::viewFrustum_extract( cascades[i].proj * cascades[i].view );
        cascadeWorldMatrices[i] = inverse( cascades[i].view );
    }

    bxGfxRenderItem_Iterator it( &rList );
    for( ; it.ok(); it.next() )
    {
        if( (it.renderMask() & renderMask ) == 0 )
            continue;

        const Matrix4& itemPose = it.worldMatrices()[0];
        const bxAABB& itemLocalBBox = it.aabb();
        const bxAABB itemWorldBBox = bxAABB::transform( itemPose, itemLocalBBox );

        for( int icascade = 0; icascade < nCascades; ++icascade )
        {
            const int collide = bxGfx::viewFrustum_AABBIntersect( frustums[icascade], itemWorldBBox.min, itemWorldBBox.max ).getAsBool();
            if( !collide )
                continue;

            const float depth = bxGfx::camera_depth( cascadeWorldMatrices[icascade], itemPose.getTranslation() ).getAsFloat();
            const u16 depth16 = depthToBits( depth );

            bxGfxSortKey_Shadow sortKey;
            sortKey.depth = depth16;
            sortKey.cascade = (u8)icascade;

            sList->add( sortKey, &rList, it.itemIndex() );
        }
    }
}


bxGfxShadows::bxGfxShadows()
    : _fxI(0)
{
    memset( _cascade, 0x00, sizeof(_cascade) );
}

void bxGfxShadows::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    const int shadowTextureWidth = bxGfx::eSHADOW_CASCADE_SIZE * bxGfx::eSHADOW_NUM_CASCADES;
    const int shadowTextureHeight = bxGfx::eSHADOW_CASCADE_SIZE;
    _depthTexture = dev->createTexture2Ddepth( shadowTextureWidth, shadowTextureHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    _fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "shadow" );
    SYS_ASSERT( _fxI != 0 );
}

void bxGfxShadows::_Shurdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI );
    dev->releaseTexture( &_depthTexture );
}

void bxGfxShadows::splitDepth( float splits[ bxGfx::eSHADOW_NUM_CASCADES], const bxGfxCamera_Params& params, float zMax, float lambda )
{
    const float N = (float)bxGfx::eSHADOW_NUM_CASCADES;
    //splits[0] = 0.f;
    //splits[bxGfx::eSHADOW_NUM_CASCADES-1] = 1.f; //params.zFar;

    const float znear = params.zNear;
    const float zfar = zMax;
    const float zRange = zfar - znear;

    const float minZ = znear; // +zRange;
    const float maxZ = znear + zRange;
    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;

    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        float p = (isplit + 1) / N;
        float log = minZ * pow( ratio, p );
        float uniform = minZ + range * p;
        float d = lambda * (log - uniform) + uniform;
        splits[isplit] = (d - znear) / zRange;
    }
}

namespace 
{
    const Matrix4 matrix_lookAt( const Vector3 &eyePos, const Vector3 &lookAtPos, const Vector3 &upVec )
    {
        Vector3 v3Y = normalize( upVec );
        const Vector3 v3Z = normalize( (eyePos - lookAtPos) );
        const Vector3 v3X = normalize( cross( v3Y, v3Z ) );
        v3Y = normalize( cross( v3Z, v3X ) );
        const Matrix4 m4EyeFrame = Matrix4( Matrix3( v3X, v3Y, v3Z ), eyePos );
        return orthoInverse( m4EyeFrame );
    }
    void _ComputeCascadeMatrices( bxGfxShadows_Cascade* cascade, const bxGfxCamera& camera, const Vector3 splitCornersWS[8], const Vector3& lightDirection )
    {
        Vector3 frustumCenter(0.f);
        for( int i = 0; i < 8; ++i )
        {
            frustumCenter += splitCornersWS[i];
            //bxGfxDebugDraw::addSphere( Vector4( splitCornersWS[i], 0.25f ), 0x0000FFFF, 1 );

        }
        frustumCenter *= 1.f / 8.f;

        const Vector3 upDir =-camera.matrix.world.getCol0().getXYZ();
        const Vector3 lightCameraPos = frustumCenter;
        const Vector3 lookAt = frustumCenter + lightDirection;
        const Matrix4 lightView = matrix_lookAt( lightCameraPos, lookAt, upDir );

        //bxGfxDebugDraw::addSphere( Vector4( lightCameraPos, 0.15f ), 0x00FF00FF, 1 );
        //bxGfxDebugDraw::addSphere( Vector4( lookAt, 0.15f ), 0x0000FFFF, 1 );

        Vector3 minExtents( FLT_MAX );
        Vector3 maxExtents(-FLT_MAX );
        for( int i = 0 ; i < 8 ; ++i )
        {
            const Vector3 cornerLS = mulAsVec4( lightView, splitCornersWS[i] );
            minExtents = minPerElem( cornerLS, minExtents );
            maxExtents = maxPerElem( cornerLS, maxExtents );
        }
        
        const floatInVec sMapSize( (float)bxGfx::eSHADOW_CASCADE_SIZE );
        const floatInVec scale( ( sMapSize + floatInVec( 9.0f ) ) / sMapSize );

        minExtents *= scale;
        maxExtents *= scale;

        const Vector3 cascadeExtents = maxExtents - minExtents;
        const Vector3 shadowCameraPos = frustumCenter + lightDirection * minExtents.getZ();

        float3_t mins, maxs, exts;
        m128_to_xyz( mins.xyz, minExtents.get128() );
        m128_to_xyz( maxs.xyz, maxExtents.get128() );
        m128_to_xyz( exts.xyz, cascadeExtents.get128() );

        const Matrix4 cascadeProj = bxGfx::cameraMatrix_ortho( mins.x, maxs.x, mins.y, maxs.y, -exts.z, exts.z );
        const Matrix4 cascadeView = matrix_lookAt( shadowCameraPos, frustumCenter, upDir );

        //const Matrix4 cameraWorld = inverse( cascadeView );
        //bxGfxDebugDraw::addBox( Matrix4::translation( shadowCameraPos), Vector3(0.5f), 0xFF0000FF, 1 );
        //bxGfxDebugDraw::addLine( shadowCameraPos, shadowCameraPos + cameraWorld.getCol2().getXYZ(), 0x0000FFFF, 1 );
        
        cascade->proj = cascadeProj;
        cascade->view = cascadeView;
    }
}///

void bxGfxShadows::computeCascades( const float splits[ bxGfx::eSHADOW_NUM_CASCADES], const bxGfxCamera& camera, const Vector3& lightDirection )
{
    const float zRange = camera.params.zFar - camera.params.zNear;

    Vector3 mainFrustumCorners[8];
    bxGfx::viewFrustum_extractCorners( mainFrustumCorners, camera.matrix.viewProj );

    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        bxGfxShadows_Cascade* cascade = &_cascade[isplit];

        const float splitNear = (isplit == 0) ? 0.f : splits[isplit - 1];
        const float splitFar = splits[isplit];

        const float splitZnear = lerp( splitNear, camera.params.zNear, camera.params.zFar );
        const float splitZfar = lerp( splitFar, camera.params.zNear, camera.params.zFar );

        const Matrix4 mainCameraSplitProj = Matrix4::perspective( camera.params.fov(), camera.params.aspect(), splitZnear, splitZfar );

        Vector3 tmpCorners[8];
        for ( int icorner = 0; icorner < 8; icorner += 2 )
        {
            const Vector3 ray = mainFrustumCorners[icorner + 1] - mainFrustumCorners[icorner];
            tmpCorners[icorner+0] = mainFrustumCorners[icorner] + ray * splitNear;//lerp( splitNear, mainFrustumCorners[icorner], mainFrustumCorners[icorner + 1] );
            tmpCorners[icorner+1] = mainFrustumCorners[icorner] + ray * splitFar;//lerp( splitFar , mainFrustumCorners[icorner], mainFrustumCorners[icorner + 1] );
        };
        
        //Vector3 tmpCorners1[8];
        //bxGfx::viewFrustum_extractCorners( tmpCorners1, mainCameraSplitProj * camera.matrix.view );
        //bxGfx::viewFrustum_debugDraw( tmpCorners, 0x0000FFFF );

        _ComputeCascadeMatrices( cascade, camera, tmpCorners, lightDirection );
        cascade->zNear_zFar = Vector4( splitZnear, splitZfar, splitNear, splitFar );

    }

    //for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    //{
    //    bxGfxShadows_Cascade* cascade = &_cascade[isplit];
    //    {
    //        const u32 colors[4] = { 0xFF0000FF, 0x00FF00FF, 0x0000FFFF, 0xFFFF00FF };
    //        bxGfx::viewFrustum_debugDraw( cascade->proj * cascade->view, colors[isplit] );
    //    }
    //}
}
