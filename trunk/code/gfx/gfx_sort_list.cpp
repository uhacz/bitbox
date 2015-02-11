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

void bxGfx::sortList_computeDepth( bxGfxSortList_Depth* sList, const bxGfxRenderList& rList, const bxGfxCamera& camera, u8 renderMask )
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
        const u16 depth16 = float_to_half_fast3( fromF32( depth ) ).u;// depthToBits( depth );

        bxGfxSortKey_Depth sortKey;
        sortKey.depth = depth16;

        sList->add( sortKey, &rList, it.itemIndex() );

        it.next();
    }
}
