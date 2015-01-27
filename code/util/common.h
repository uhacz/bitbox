#pragma once

#include "type.h"

#define PI 3.14159265358979323846f

template<typename Type>
inline Type minOfPair( const Type& a, const Type& b ) {	return ( a < b ) ? a : b; }

template<typename Type>
inline Type maxOfPair( const Type& a, const Type& b ) {	return ( a < b ) ? b : a; }

template<typename Type>
inline Type clamp( const Type& x, const Type& a, const Type& b )
{
    return maxOfPair( a, minOfPair(b,x) );
}
inline float saturate( float x )
{
    return clamp( x, 0.f, 1.f );
}

i32  ifloorf( const float x );
inline i32 iceil( const i32 x, const i32 y )
{
    return ( 1 + ( (x-1) / y ) );
}

inline float frac( const float value ) 
{
    return value - float( (int)value );
}

inline float select( float x, float a, float b )
{
    return ( x>=0 ) ? a : b;
}

inline float linearstep( float edge0, float edge1, float x )
{
    return clamp( (x - edge0)/(edge1 - edge0), 0.f, 1.f );
}
inline float smoothstep( float edge0, float edge1, float x )
{
    // Scale, bias and saturate x to 0..1 range
    x = clamp( (x - edge0)/(edge1 - edge0), 0.f, 1.f ); 
    // Evaluate polynomial
    return x*x*(3.f - 2.f*x);
}

inline float smootherstep( float edge0, float edge1, float x )
{
    // Scale, and clamp x to 0..1 range
    x = clamp( (x - edge0)/(edge1 - edge0), 0.f, 1.f );
    // Evaluate polynomial
    return x*x*x*(x*(x*6.f - 15.f) + 10.f);
}

inline float lerp( float t, float a, float b )
{
    return a + t*( b - a );
}

inline u16 depthToBits( float depth )
{
    union { float f; unsigned i; } f2i;
    f2i.f = depth;
    unsigned b = f2i.i >> 22; // take highest 10 bits
    return (u16)b;
}
