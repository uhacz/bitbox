#pragma once

#include <util/vectormath/vectormath.h>
#include <gdi/gdi_context.h>
#include "gfx_type.h"
#include "gfx_sort_list.h"

struct bxGfxCamera;
struct bxGfxCamera_Params;
struct bxGdiContext;
struct bxGfxRenderList;
class bxResourceManager;

////
////
struct bxGfxShadows_Cascade
{
    Matrix4 view;
    Matrix4 proj;
    Vector4 zNear_zFar;
};

////
////
union bxGfxSortKey_Shadow
{
    u32 hash;
    struct  
    {
        u16 depth;
        u8 cascade;
    };
};
typedef bxGfxSortList< bxGfxSortKey_Shadow> bxGfxSortList_Shadow;
namespace bxGfx
{
    void sortList_computeShadow( bxGfxSortList_Shadow* sList, const bxGfxRenderList& rList, const bxGfxShadows_Cascade* cascades, int nCascades, u8 renderMask = bxGfx::eRENDER_MASK_SHADOW );
}

////
////
struct bxGfxShadows 
{
    void splitDepth( float splits[ bxGfx::eSHADOW_NUM_CASCADES], const bxGfxCamera_Params& params, float zMax, float lambda );
    void computeCascades( const float splits[ bxGfx::eSHADOW_NUM_CASCADES], const bxGfxCamera& camera, const Vector3& lightDirection );

    bxGfxShadows();
    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void _Shurdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void _ShowGUI();

public:
    bxGfxShadows_Cascade _cascade[ bxGfx::eSHADOW_NUM_CASCADES ];
    bxGdiTexture _depthTexture;
    bxGdiShaderFx_Instance* _fxI;


    struct Params
    {
        f32 bias;
        f32 normalOffsetScale[bxGfx::eSHADOW_NUM_CASCADES];
        i32 kernelSize;
        
        u32 flag_useNormalOffset;
        u32 flag_showCascades;
        

        Params()
            : bias( 0.0001f )
            , kernelSize( 7 )
            , flag_useNormalOffset(1)
            , flag_showCascades( 0 )
        {
            for( int i = 0;  i < bxGfx::eSHADOW_NUM_CASCADES; ++i )
            {
                normalOffsetScale[i] = -0.05f;
            }
        }
    } _params;
};



