#pragma once

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <util/vectormath/vectormath.h>

namespace bxGfxDebugDraw
{
    void _Startup( bxGdiDeviceBackend* dev );
    void _Shutdown( bxGdiDeviceBackend* dev );

    void addSphere( const Vector4& pos_radius, u32 colorRGBA, int depth );
    void addBox( const Matrix4& pose, const Vector3& ext, u32 colorRGBA, int depth );
    void addLine( const Vector3& pointA, const Vector3& pointB, u32 colorRGBA, int depth );
    void addAxes( const Matrix4& pose );
    
    void addFrustum( const Matrix4& viewProj, u32 colorRGBA, int depth );
    void addFrustum( const Vector3 corners[8], u32 colorRGBA, int depth );

    void flush( bxGdiContext* ctx, const Matrix4& viewProj );

}///
