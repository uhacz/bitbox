#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>

namespace bx{ namespace rdi
{
    struct CommandQueue;

namespace debug_draw
{
    void _Startup ();
    void _Shutdown();
    void _Flush( CommandQueue* cmdq, const Matrix4& view, const Matrix4& proj );

    void AddSphere ( const Vector4& pos_radius, u32 colorRGBA, int depth );
    void AddBox    ( const Matrix4& pose, const Vector3& ext, u32 colorRGBA, int depth );
    void AddLine   ( const Vector3& pointA, const Vector3& pointB, u32 colorRGBA, int depth );
    void AddAxes   ( const Matrix4& pose );
    void AddFrustum( const Matrix4& viewProj, u32 colorRGBA, int depth );
    void AddFrustum( const Vector3 corners[8], u32 colorRGBA, int depth );

    void AddSphere ( const Vector4F& pos_radius, u32 colorRGBA, int depth );
    void AddBox    ( const Matrix4F& pose, const Vector3F& ext, u32 colorRGBA, int depth );
    void AddLine   ( const Vector3F& pointA, const Vector3F& pointB, u32 colorRGBA, int depth );
    void AddAxes   ( const Matrix4F& pose );
    void AddFrustum( const Matrix4F& viewProj, u32 colorRGBA, int depth );
    void AddFrustum( const Vector3F corners[8], u32 colorRGBA, int depth );

}//
}}///
