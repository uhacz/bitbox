#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;

namespace bx
{
    struct GfxContext;
    struct GfxCommandQueue;

    struct GfxMeshInstance;
    struct GfxCamera;
    struct GfxScene;
    struct GfxViewport
    {
        i16 x, y;
        u16 w, h;
        GfxViewport() {}
        GfxViewport( int xx, int yy, unsigned ww, unsigned hh )
            : x( xx ), y( yy ), w( ww ), h( hh ) {}
    };

    struct GfxMeshInstanceData
    {
        enum
        {
            eMASK_RENDER_SOURCE,
            eMASK_SHADER_FX,
            eMASK_LOCAL_AABB,
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
}///