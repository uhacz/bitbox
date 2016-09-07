#pragma once

#include "type.h"
#include "vectormath/vectormath.h"

namespace bx
{

    struct Curve3D
    {
        Vector3*    _points { nullptr };
        f32*        _knots { nullptr };
        u32         _size{ 0 };
        u32         _capacity{ 0 };
    };
    struct Curve1D
    {
        f32* _points{ nullptr };
        f32* _knots{ nullptr };
        u32  _size{ 0 };
        u32  _capacity{ 0 };
    };


    namespace curve
    {
        void allocate( Curve3D& cv, u32 size );
        void deallocate( Curve3D& cv );

        void clear( Curve3D& cv );
        u32 push_back( Curve3D& cv, const Vector3& point, f32 t );

        inline u32 size( const Curve3D& cv ) { return cv._size; }

        const Vector3 evaluate_catmullrom( const Curve3D& cv, f32 t );
        const Matrix4 compute_pose_catmullrom( const Curve3D& cv, f32 t, f32 delta = 0.0001f );

        //////////////////////////////////////////////////////////////////////////
        void allocate( Curve1D& cv, u32 size );
        void deallocate( Curve1D& cv );
        void clear( Curve1D& cv );
        u32 push_back( Curve1D& cv, f32 point, f32 t );
        inline u32 size( const Curve1D& cv ) { return cv._size; }
        float evaluate_catmullrom( const Curve1D& cv, f32 t );
    }//
}//