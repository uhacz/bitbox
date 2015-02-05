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
        //splits[isplit] = lambda * znear * powf(zfar / znear, isplit / N) + (1.0f - lambda) * ((znear + (isplit / N)) * (zfar - znear));
    }
}

namespace 
{
    void _ComputeCascadeMatrices( bxGfxShadows_Cascade* cascade, const bxGfxCamera& camera, const Vector3 splitCornersWS[8], const Vector3& lightDirection )
    {
        Vector3 frustumCenter(0.f);
        for( int i = 0; i < 8; ++i )
        {
            frustumCenter += splitCornersWS[i];

        }
        frustumCenter *= 1.f / 8.f;

        const Vector3 upDir =-camera.matrix.world.getCol0().getXYZ();
        const Vector3 lightCameraPos = frustumCenter;
        const Vector3 lookAt = frustumCenter + lightDirection;
        const Matrix4 lightView = Matrix4::lookAt( Point3( lightCameraPos ), Point3( lookAt ), upDir );

        bxGfxDebugDraw::addSphere( Vector4( lightCameraPos, 0.15f ), 0x00FF00FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( lookAt, 0.15f ), 0x0000FFFF, 1 );

        Vector3 minExtents( FLT_MAX );
        Vector3 maxExtents(-FLT_MAX );
        for( int i = 0 ; i < 8 ; ++i )
        {
            const Vector3 cornerLS = mulAsVec4( lightView, splitCornersWS[i] );
            minExtents = minPerElem( cornerLS, minExtents );
            maxExtents = maxPerElem( cornerLS, maxExtents );
        }
        
        const Vector3 cascadeExtents = maxExtents - minExtents;
        const Vector3 shadowCameraPos = frustumCenter + lightDirection * minExtents.getZ();

        float3_t mins, maxs, exts;
        m128_to_xyz( mins.xyz, minExtents.get128() );
        m128_to_xyz( maxs.xyz, maxExtents.get128() );
        m128_to_xyz( exts.xyz, cascadeExtents.get128() );

        const Matrix4 cascadeProj = bxGfx::cameraMatrix_ortho( mins.x, maxs.x, mins.y, maxs.y, 0.f, exts.z );
        const Matrix4 cascadeView = Matrix4::lookAt( Point3( shadowCameraPos ), Point3( frustumCenter ), upDir );

        const Matrix4 cameraWorld = inverse( cascadeView );
        bxGfxDebugDraw::addBox( Matrix4::translation( shadowCameraPos), Vector3(0.5f), 0xFF0000FF, 1 );
        bxGfxDebugDraw::addLine( shadowCameraPos, shadowCameraPos + cameraWorld.getCol2().getXYZ(), 0x0000FFFF, 1 );
        
        cascade->proj = cascadeProj;
        cascade->view = cascadeView;
    }
    void _ComputeCascade( bxGfxShadows_Cascade* cascade, const bxGfxCamera& camera, float nearSplitAlpha, float farSplitAlpha, float zRange, const Vector3& lightDirection )
    {
        Vector3 frustumCornersWS[8];
        bxGfx::viewFrustum_extractCorners( frustumCornersWS, camera.matrix.viewProj );


        Vector3 splitCornersWS[8];
        for ( int i = 0; i < 8; i+=2 )
        {
            
            const Vector3 cornerRay = frustumCornersWS[i + 1] - frustumCornersWS[i];
            const Vector3 nearCornerRay = cornerRay * nearSplitAlpha;
            const Vector3 farCornerRay = cornerRay * farSplitAlpha;

            splitCornersWS[i + 1] = frustumCornersWS[i] + farCornerRay;
            splitCornersWS[i]     = frustumCornersWS[i] + nearCornerRay;


            bxGfxDebugDraw::addSphere( Vector4( splitCornersWS[i], 0.25f ), 0xFF00FFFF, 1 );
            bxGfxDebugDraw::addSphere( Vector4( splitCornersWS[i+1], 0.25f ), 0xFF00FFFF, 1 );
        }

        Vector3 frustumCenter(0.f);
        for( int i = 0; i < 8; ++i )
        {
            frustumCenter += splitCornersWS[i];
            
        }
        frustumCenter *= 1.f / 8.f;

        const Vector3 upDir = camera.matrix.world.getCol0().getXYZ();
        const Vector3 lightCameraPos = frustumCenter;
        const Vector3 lookAt = frustumCenter + lightDirection;
        const Matrix4 lightView = Matrix4::lookAt( Point3( lightCameraPos ), Point3( lookAt ), upDir );

        bxGfxDebugDraw::addSphere( Vector4( lightCameraPos, 0.5f ), 0x00FF00FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( lookAt, 0.5f ), 0x0000FFFF, 1 );

        Vector3 minExtents( FLT_MAX );
        Vector3 maxExtents(-FLT_MAX );
        Vector3 minExtentsWS( FLT_MAX );
        Vector3 maxExtentsWS(-FLT_MAX );

        for( int i = 0 ; i < 8 ; ++i )
        {
            minExtentsWS = minPerElem( minExtentsWS, splitCornersWS[i] );
            maxExtentsWS = maxPerElem( maxExtentsWS, splitCornersWS[i] );
            const Vector3 cornerLS = mulAsVec4( lightView, splitCornersWS[i] );
            minExtents = minPerElem( cornerLS, minExtents );
            maxExtents = maxPerElem( cornerLS, maxExtents );
        }

        //{
        //    const Vector3 minExtentsTmp = mulAsVec4( lightView, minExtentsWS );
        //    const Vector3 maxExtentsTmp = mulAsVec4( lightView, maxExtentsWS );
        //    minExtents = minPerElem( minExtentsTmp, maxExtentsTmp );
        //    maxExtents = maxPerElem( minExtentsTmp, maxExtentsTmp );

        //    bxAABB aabb ( minExtentsWS, maxExtentsWS );
        //    bxGfxDebugDraw::addBox( Matrix4::translation( bxAABB::center( aabb ) ), bxAABB::size(aabb)*halfVec, 0xFFFFFFFF, true );

        //}



        //minExtents = mulPerElem( minExtents, Vector3( 2.1f, 2.1f, 2.f ) );
        //maxExtents = mulPerElem( maxExtents, Vector3( 2.1f, 2.1f, 2.f ) );

        const Vector3 cascadeExtents = maxExtents - minExtents;
        const Vector3 shadowCameraPos = frustumCenter - lightDirection * -minExtents.getZ();

        float3_t mins, maxs, exts;
        m128_to_xyz( mins.xyz, minExtents.get128() );
        m128_to_xyz( maxs.xyz, maxExtents.get128() );
        m128_to_xyz( exts.xyz, cascadeExtents.get128() );
        
        const Matrix4 cascadeProj = bxGfx::cameraMatrix_ortho( mins.x, maxs.x, mins.y, maxs.y, 0.0f, exts.z );
        const Matrix4 cascadeView = Matrix4::lookAt( Point3( shadowCameraPos ), Point3( frustumCenter ), upDir );

        bxGfxDebugDraw::addSphere( Vector4( shadowCameraPos, 0.5f ), 0xFF0000FF, 1 );

        cascade->proj = cascadeProj;
        cascade->view = cascadeView;
        cascade->zNear_zFar = Vector4( nearSplitAlpha * zRange, farSplitAlpha * zRange, 0.f, 0.f );

        //bxGfxCamera_Params params = camera.params;
        //params.zNear = zNear;
        //params.zFar = zFar;

        //const Matrix4 splitProj = bxGfx::cameraMatrix_projection( params, bxGfx::eSHADOW_CASCADE_SIZE, bxGfx::eSHADOW_CASCADE_SIZE );
        //const Matrix4 splitViewProj = splitProj * camera.matrix.view;

        //bxGfx::viewFrustum_debugDraw( splitViewProj, 0xFF0000FF );

        //Vector3 frustumCorners[8];
        //bxGfx::viewFrustum_extractCorners( frustumCorners, splitViewProj );

        //Vector3 frustumCentroid(0.f);
        //for( int i = 0; i < 8; ++i )
        //{
        //    frustumCentroid += frustumCorners[i];
        //    //bxGfxDebugDraw::addSphere( Vector4( frustumCorners[i], 0.5f ), 0xFF0000FF, 1 );
        //}
        //frustumCentroid *= 1.f / 8.f;

        //const float distFromCentroid = maxOfPair( zFar - zNear, length( frustumCorners[5] - frustumCorners[4] ).getAsFloat() ) + 50.f;
        //const Matrix4 viewMatrix = Matrix4::lookAt( Point3( frustumCentroid ) - ( lightDirection * distFromCentroid ), Point3( frustumCentroid ), Vector3::yAxis() );

        //bxGfxDebugDraw::addSphere( Vector4( frustumCentroid, 0.5f ), 0xFF00FFFF, 1 );

        //Vector3 frustumConrnersLS[8];
        //for( int i = 0; i < 8; ++i )
        //{
        //    frustumConrnersLS[i] = mulAsVec4( viewMatrix, frustumCorners[i] );
        //}

        //Vector3 minVectorLS = frustumConrnersLS[0];
        //Vector3 maxVectorLS = frustumConrnersLS[0];
        //for( int i = 0; i < 8; ++i )
        //{
        //    minVectorLS = minPerElem( minVectorLS, frustumConrnersLS[i] );
        //    maxVectorLS = maxPerElem( maxVectorLS, frustumConrnersLS[i] );
        //}

        //float3_t min, max;
        //m128_to_xyz( min.xyz, minVectorLS.get128() );
        //m128_to_xyz( max.xyz, maxVectorLS.get128() );

        //const float nearClipOffset = 100.f;
        //const Matrix4 projMatrix = Matrix4::orthographic( min.x, max.x, min.y, max.y, -max.z - nearClipOffset, -min.z );
        //       
        //cascade->proj = sc * tr * projMatrix;
        //cascade->view = viewMatrix;
        //cascade->zNear_zFar = Vector4( zNear, zFar, 0.f, 0.f );
    }
}///

void bxGfxShadows::computeCascades( const float splits[ bxGfx::eSHADOW_NUM_CASCADES], const bxGfxCamera& camera, const Vector3& lightDirection )
{
    const float zRange = camera.params.zFar - camera.params.zNear;
    
    {
        bxGfx::viewFrustum_debugDraw( camera.matrix.viewProj, 0x0000FFFF );
    }

    Vector3 frustumCornersWS[8];
    bxGfx::viewFrustum_extractCorners( frustumCornersWS, camera.matrix.viewProj );

    /*
        0   3
        1   2
    */

    //const Vector3 rays[] = 
    //{
    //    frustumCornersWS[1] - frustumCornersWS[0],
    //    frustumCornersWS[3] - frustumCornersWS[2],
    //    frustumCornersWS[5] - frustumCornersWS[4],
    //    frustumCornersWS[7] - frustumCornersWS[6],
    //};

    Vector3 splitCornersWS[4 * (bxGfx::eSHADOW_NUM_CASCADES+1) ];
    splitCornersWS[0] = frustumCornersWS[0];
    splitCornersWS[1] = frustumCornersWS[2];
    splitCornersWS[2] = frustumCornersWS[4];
    splitCornersWS[3] = frustumCornersWS[6];
    
    Vector4 splitClipPlanes[bxGfx::eSHADOW_NUM_CASCADES];
    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        const float splitNear = (isplit==0) ? 0.f : splits[isplit-1];
        const float splitFar = splits[isplit];
        const int cornerBeginIdx = (isplit+1) * 4;
        
        splitCornersWS[cornerBeginIdx + 0] = lerp( splitFar, frustumCornersWS[0], frustumCornersWS[1] );
        splitCornersWS[cornerBeginIdx + 1] = lerp( splitFar, frustumCornersWS[2], frustumCornersWS[3] );
        splitCornersWS[cornerBeginIdx + 2] = lerp( splitFar, frustumCornersWS[4], frustumCornersWS[5] );
        splitCornersWS[cornerBeginIdx + 3] = lerp( splitFar, frustumCornersWS[6], frustumCornersWS[7] );

        const float splitZnear = lerp( splitNear, camera.params.zNear, camera.params.zFar );
        const float splitZfar  = lerp( splitFar , camera.params.zNear, camera.params.zFar );

        splitClipPlanes[isplit] = Vector4( splitZnear, splitZfar, 0.f, 0.f );
    }
    
//     for( int i = 0 ;i < 4 * (bxGfx::eSHADOW_NUM_CASCADES+1); ++i )
//     {
//         bxGfxDebugDraw::addSphere( Vector4( splitCornersWS[i], 0.25f ), 0xFFFFFFFF, true );
//     }

    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        bxGfxShadows_Cascade* cascade = &_cascade[isplit];
        const int cornerBeginIdx = isplit * 4;

        _ComputeCascadeMatrices( cascade, camera, splitCornersWS + cornerBeginIdx, lightDirection );
        cascade->zNear_zFar = splitClipPlanes[isplit];
    }


    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        bxGfxShadows_Cascade* cascade = &_cascade[isplit];
        //const float zNear = (isplit==0) ? 0.f : splits[isplit-1];
        //const float zFar  = splits[isplit];
        //
        //_ComputeCascade( cascade, camera, zNear, zFar, zRange, lightDirection );


        {
            const u32 colors[4] = { 0xFF0000FF, 0x00FF00FF, 0x0000FFFF, 0xFFFF00FF };
            bxGfx::viewFrustum_debugDraw( cascade->proj * cascade->view, colors[isplit] );
        }
    }
}
