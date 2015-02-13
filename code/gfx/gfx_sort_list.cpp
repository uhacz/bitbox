#include "gfx_sort_list.h"
#include "gfx_render_list.h"
#include "gfx_camera.h"

#include <gdi/gdi_shader.h>
#include <gdi/gdi_render_source.h>
#include <util/common.h>
#include <util/float16.h>

void bxGfx::sortList_computeColor( bxGfxSortList_Color* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask )
{
    const bxGfxViewFrustum frustum = bxGfx::viewFrustum_extract( camera.matrix.viewProj );

    bxGfxRenderItem_Iterator it( &rList );
    while ( it.ok() )
    {
        const Matrix4& itemPose = it.worldMatrices()[0];
        const bxAABB& itemLocalBBox = it.aabb();

        const bxAABB itemWorldBBox = bxAABB::transform( itemPose, itemLocalBBox );

        const int inFrustum = bxGfx::viewFrustum_AABBIntersect( frustum, itemWorldBBox.min, itemWorldBBox.max ).getAsBool();

        const int itemValid = inFrustum && (renderMask & it.renderMask() );
        if ( !itemValid )
        {
            it.next();
            continue;
        }

        bxGfxShadingPass shPass = it.shadingPass();
        const u8 itemLayer = it.renderLayer();
        //const float depth = bxGfx::camera_depth( camera.matrix.world, itemPose.getTranslation() ).getAsFloat();
        //const u16 depth16 = depthToBits( depth );
        const u32 shaderHash = shPass.fxI->sortHash( shPass.passIndex );

        bxGfxSortKey_Color sortKey;
        sortKey.shader = shaderHash;
        sortKey.mesh = it.renderSource()->sortHash;
        //sortKey.depth = depth16;
        sortKey.layer = itemLayer;

        sList->add( sortKey, &rList, it.itemIndex() );

        it.next();
    }

}

void bxGfx::sortList_computeDepth( bxGfxSortList_Depth* sList, float minZ_maxZ[2], const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask )
{
    const bxGfxViewFrustum frustum = bxGfx::viewFrustum_extract( camera.matrix.viewProj );

    bxGfxRenderItem_Iterator it( &rList );
    while ( it.ok() )
    {
        if ( (it.renderMask() & renderMask) == 0 )
        {
            it.next();
            continue;
        }
        
        const Matrix4& itemPose = it.worldMatrices()[0];
        const bxAABB& itemLocalBBox = it.aabb();

        const bxAABB itemWorldBBox = bxAABB::transform( itemPose, itemLocalBBox );

        const int inFrustum = bxGfx::viewFrustum_AABBIntersect( frustum, itemWorldBBox.min, itemWorldBBox.max ).getAsBool();
        if ( !inFrustum )
        {
            it.next();
            continue;
        }

        const float depth = bxGfx::camera_depth( camera.matrix.world, itemPose.getTranslation() ).getAsFloat();
        
        bxGfxSortKey_Depth sortKey;
        sortKey.depth = float_to_half_fast3( fromF32( depth ) ).u;// depthToBits( depth );
        sList->add( sortKey, &rList, it.itemIndex() );

        const Vector3 bboxCenter = bxAABB::center( itemWorldBBox );
        const Vector3 bboxSize = bxAABB::size( itemWorldBBox );
        //const floatInVec bsRadius = length( bboxSize ) * halfVec;

        //const float minD = bxGfx::camera_depth( camera.matrix.world, bboxCenter - camera.matrix.worldDir()*bsRadius ).getAsFloat();
        //const float maxD = bxGfx::camera_depth( camera.matrix.world, bboxCenter + camera.matrix.worldDir()*bsRadius ).getAsFloat();
        //minZ_maxZ[0] = minOfPair( minD, minZ_maxZ[0] );
        //minZ_maxZ[1] = maxOfPair( maxD, minZ_maxZ[1] );

        const Vector3 bboxCorenrs[8] = 
        {
            itemWorldBBox.min,
            itemWorldBBox.min + Vector3( zeroVec, bboxSize.getY(), zeroVec ),
            itemWorldBBox.min + Vector3( bboxSize.getX(), zeroVec, zeroVec ),
            itemWorldBBox.min + Vector3( bboxSize.getX(), bboxSize.getY(), zeroVec ),

            itemWorldBBox.max,
            itemWorldBBox.max - Vector3( zeroVec, bboxSize.getY(), zeroVec ),
            itemWorldBBox.max - Vector3( bboxSize.getX(), zeroVec, zeroVec ),
            itemWorldBBox.max - Vector3( bboxSize.getX(), bboxSize.getY(), zeroVec ),
        };
        for( int i = 0; i < 8; ++i )
        {
            const float d = bxGfx::camera_depth( camera.matrix.world, bboxCorenrs[i] ).getAsFloat();
            minZ_maxZ[0] = minOfPair( d, minZ_maxZ[0] );
            minZ_maxZ[1] = maxOfPair( d, minZ_maxZ[1] );
        }
        //const float bboxMinDepth = bxGfx::camera_depth( camera.matrix.world, itemWorldBBox.min ).getAsFloat();
        //const float bboxMaxDepth = bxGfx::camera_depth( camera.matrix.world, itemWorldBBox.max ).getAsFloat();
        //{
        //    const Vector3 minCS_0 = mulPerElem( mulAsVec4( camera.matrix.view, itemWorldBBox.min ), Vector3(1.f,1.f,-1.f) );
        //    const Vector3 maxCS_0 = mulPerElem( mulAsVec4( camera.matrix.view, itemWorldBBox.max ), Vector3(1.f,1.f,-1.f) );
        //    
        //    const Vector3 minCS = minPerElem( minCS_0, maxCS_0 );
        //    const Vector3 maxCS = maxPerElem( minCS_0, maxCS_0 );
        //    
        //    //const float a = minOfPair( bboxMinDepth, bboxMaxDepth );
        //    //const float b = maxOfPair( bboxMinDepth, bboxMaxDepth );
        //    minZ_maxZ[0] = minOfPair( minCS.getZ().getAsFloat(), minZ_maxZ[0] );
        //    minZ_maxZ[1] = maxOfPair( maxCS.getZ().getAsFloat(), minZ_maxZ[1] );
        //}

        

        it.next();
    }
}
