#pragma once

#include <util/camera.h>
#include <util/view_frustum.h>

struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;

namespace bx
{
    struct GfxContext;
    struct GfxCommandQueue;

    struct GfxMeshInstance;
    struct GfxCamera;
    struct GfxScene;
    typedef bx::gfx::Viewport GfxViewport;
    typedef bx::gfx::ViewFrustum GfxViewFrustum;

    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxMeshInstanceData
    {
        enum
        {
            eMASK_RENDER_SOURCE = 0x1,
            eMASK_SHADER_FX = 0x2,
            eMASK_LOCAL_AABB = 0x4,
        };
        bxGdiRenderSource* rsource;
        bxGdiShaderFx_Instance* fxInstance;
        f32 localAABB[6];
        u32 mask;

        GfxMeshInstanceData();

        GfxMeshInstanceData& renderSourceSet( bxGdiRenderSource* rsrc );
        GfxMeshInstanceData& fxInstanceSet( bxGdiShaderFx_Instance* fxI );
        GfxMeshInstanceData& locaAABBSet( const Vector3& pmin, const Vector3& pmax );
    };

    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxGlobalResources
    {
        struct{
            bxGdiShaderFx_Instance* utils;
            bxGdiShaderFx_Instance* texUtils;
        } fx;

        struct{
            bxGdiRenderSource* fullScreenQuad;
            bxGdiRenderSource* sphere;
            bxGdiRenderSource* box;
        } mesh;
    };
}///