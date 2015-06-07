#pragma once

#include "vectormath/vectormath.h"
#include "type.h"

struct bxAABB
{
    bxAABB() {}
    bxAABB( const Vector3& minp, const Vector3& maxp )
        : min(minp), max(maxp) {}

    Vector3 min;
    Vector3 max;

    static inline bxAABB prepare()
    {
        return bxAABB( Vector3( FLT_MAX ), Vector3( -FLT_MAX ) );
    }
    static inline Vector3 size( const bxAABB& bbox )
    {
        return bbox.max - bbox.min;
    }
    static inline Vector3 center( const bxAABB& bbox )
    {
        return bbox.min + (bbox.max - bbox.min) * 0.5f;
    }

    static inline bxAABB extend( const bxAABB& bbox, const Vector3& point )
    {
        return bxAABB( minPerElem( bbox.min, point ), maxPerElem( bbox.max, point ) );
    }

    static inline bxAABB merge( const bxAABB& a, const bxAABB& b )
    {
        return bxAABB( minPerElem( a.min, b.min ), maxPerElem( a.max, b.max ) );
    }

    static inline bxAABB transform( const Matrix4& matrix, const bxAABB& bbox )
    {
        Vector4 xa = matrix.getCol0() * bbox.min.getX();
        Vector4 xb = matrix.getCol0() * bbox.max.getX();

        Vector4 ya = matrix.getCol1() * bbox.min.getY();
        Vector4 yb = matrix.getCol1() * bbox.max.getY();

        Vector4 za = matrix.getCol2() * bbox.min.getZ();
        Vector4 zb = matrix.getCol2() * bbox.max.getZ();

        bxAABB result;
        result.min = ( minPerElem( xa, xb ) + minPerElem( ya, yb ) + minPerElem( za, zb ) ).getXYZ() + matrix.getTranslation();
        result.max = ( maxPerElem( xa, xb ) + maxPerElem( ya, yb ) + maxPerElem( za, zb ) ).getXYZ() + matrix.getTranslation();

        return result;
    }

    static inline vec_float4 pointInAABBf4( const vec_float4 bboxMin, const vec_float4 bboxMax, const vec_float4 point )
    {
        const vec_float4 a = vec_cmplt( bboxMin, point );
        const vec_float4 b = vec_cmpgt( bboxMax, point );
        const vec_float4 a_n_b = vec_and( a, b );
        return vec_and( vec_splat( a_n_b, 0 ), vec_and( vec_splat( a_n_b, 1 ), vec_splat( a_n_b, 2 ) ) );
    }

    static inline bool isPointInside( const bxAABB& aabb, const Vector3& point )
    {
        //return boolInVec( pointInAABBf4( picoF128( bboxMin.get128() ), picoF128( bboxMax.get128() ), picoF128( point.get128() ) ) );
        return boolInVec( floatInVec( pointInAABBf4( aabb.min.get128(), aabb.max.get128(), point.get128() ) ) ).getAsBool();
    }

    static inline Vector3 closestPointOnABB( const Vector3& bboxMin, const Vector3& bboxMax, const Vector3& point )
    {
        return minPerElem( maxPerElem( point, bboxMin ), bboxMax );
    }

};
