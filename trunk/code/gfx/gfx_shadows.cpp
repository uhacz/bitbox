#include "gfx_shadows.h"
#include "gfx_camera.h"
#include "gfx_render_list.h"
#include "gfx_sort_list.h"

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
    _depthTexture = dev->createTexture2Ddepth( shadowTextureWidth, shadowTextureHeight, 1, bxGdi::eTYPE_DEPTH16, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    _fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "shadow" );
    SYS_ASSERT( _fxI != 0 );
}

void bxGfxShadows::_Shurdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI );
    dev->releaseTexture( &_depthTexture );
}

void bxGfxShadows::splitDepth( float splits[ bxGfx::eSHADOW_NUM_CASCADES+1], const bxGfxCamera_Params& params, float zMax, float lambda )
{
    const float N = (float)bxGfx::eSHADOW_NUM_CASCADES;
    splits[0] = params.zNear;
    splits[bxGfx::eSHADOW_NUM_CASCADES] = zMax; //params.zFar;

    const float znear = params.zNear;
    const float zfar = zMax;

    for( int isplit = 1; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        splits[isplit] = lambda * znear * powf(zfar / znear, isplit / N) + (1.0f - lambda) * ((znear + (isplit / N)) * (zfar - znear));
    }
}

namespace 
{
    void _ComputeCascade( bxGfxShadows_Cascade* cascade, const bxGfxCamera& camera, float zNear, float zFar, const Vector3& lightDirection )
    {
        
    }
}///

void bxGfxShadows::computeCascades( const float splits[ bxGfx::eSHADOW_NUM_CASCADES+1], const bxGfxCamera& camera, const Vector3& lightDirection )
{
    for( int isplit = 0; isplit < bxGfx::eSHADOW_NUM_CASCADES; ++isplit )
    {
        bxGfxShadows_Cascade* cascade = &_cascade[isplit];
        const float zNear = splits[isplit];
        const float zFar  = splits[isplit+1];
        
        _ComputeCascade( cascade, camera, zNear, zFar, lightDirection );
    }
}

void bxGfxShadows::shadowMapDraw( bxGdiContext* ctx, bxGfxRenderList** rLists, int nLists )
{

}

