#pragma once

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <util/vectormath/vectormath.h>

namespace bxGfxDebugDraw
{
    void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void addSphere( const Vector4& pos_radius, u32 colorRGBA, int depth );
    void addBox( const Matrix4& pose, const Vector3& ext, u32 colorRGBA, int depth );
    void addLine( const Vector3& pointA, const Vector3& pointB, u32 colorRGBA, int depth );
    
    void flush( bxGdiContext* ctx, const Matrix4& viewProj );

}///
