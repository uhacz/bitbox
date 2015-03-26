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

    static inline bool isPointInside( const bxAABB& aabb, const Vector3& point )
    {
        const boolInVec xInside = aabb.min.getX() < point.getX() & aabb.max.getX() > point.getX();
        const boolInVec yInside = aabb.min.getY() < point.getY() & aabb.max.getY() > point.getY();
        const boolInVec zInside = aabb.min.getZ() < point.getZ() & aabb.max.getZ() > point.getZ();

        return ((xInside & yInside & zInside).getAsBool());
    }
};
