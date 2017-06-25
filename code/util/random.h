#pragma once

#include "type.h"
#include <util/vectormath/vectormath.h>

struct bxRandomGen
{
    bxRandomGen()
    {}
    bxRandomGen( unsigned sed )
    {
        _seed = sed;
        _rand = sed;
        _m = 1770875;
        _a = 2416;
        _c = 374441;
        _maxRand = _m - 1;
        _maxRandInv = 1.0f / (float)( _maxRand );
    }

    u32 _seed;
    u32 _rand;

    u32 _maxRand;
    f32 _maxRandInv;

    u32 _a;
    u32 _c;
    u32 _m;

    inline void reset()
    {
        _rand = _seed;
    }

    inline unsigned get()
    {
        return _rand = ( _a * _rand + _c ) % _m;
    }

    inline unsigned get0n( u32 n )
    {
        if( n == 0 ) return 0;
        return (_rand = ( _a*_rand + _c ) % _m) % n;
    }
    inline float getf( float max = 1.0f ) 
    {
        return max * _maxRandInv * get();
    }

    inline float getf( float min, float max ) 
    { 
        return min + (max - min) * _maxRandInv * get();
    }
};

namespace bxRand
{
    inline Vector3 randomVector3( bxRandomGen& rnd, const Vector3& min_vec, const Vector3& max_vec )
    {
        const float r0 = (float)rnd.get();
        const float r1 = (float)rnd.get();
        const float r2 = (float)rnd.get();
        return min_vec + mulPerElem( max_vec - min_vec, Vector3( r0, r1, r2 ) * rnd._maxRandInv );
    }
    inline Vector3 randomVector2( bxRandomGen& rnd, const Vector3& min_vec, const Vector3& max_vec )
    {
        const float r0 = (float)rnd.get();
        const float r1 = (float)rnd.get();
        return min_vec + mulPerElem( max_vec - min_vec, Vector3( r0, r1, 0.f ) * rnd._maxRandInv );
    }
    inline float randomFloat( bxRandomGen& rnd, float base_value, float randomization )
    {
        const float min_value = base_value * ( 1.f - randomization );
        const float max_value = base_value * ( 1.f + randomization );
        return rnd.getf( min_value, max_value );
    }

    // returns a random unit vector (also can add an offset to generate around an off axis vector)
    inline Vector3F unitVector( bxRandomGen& rnd )
    {
        float phi = rnd.getf( PI2 );
        float theta = rnd.getf( PI2 );

        float cosTheta = ::cosf( theta );
        float sinTheta = ::sinf( theta );

        float cosPhi = ::cosf( phi );
        float sinPhi = ::sinf( phi );

        return Vector3F( cosTheta*sinPhi, cosPhi, sinTheta*sinPhi );
    }
}///
