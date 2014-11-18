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
};
