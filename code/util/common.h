#pragma once

#include "type.h"
#include "debug.h"
#include <math.h>

#define PI 3.14159265358979323846f
#define PI2 6.28318530717958647693f
#define PI_HALF 1.57079632679489661923f
#define PI_INV 0.31830988618379067154f
#define PI_OVER_180 0.01745329251994329577f

inline int bitcount( unsigned int n )
{
    /* works for 32-bit numbers only    */
    /* fix last line for 64-bit numbers */

    unsigned int tmp;
    tmp = n - ( ( n >> 1 ) & 033333333333 ) - ( ( n >> 2 ) & 011111111111 );
    return ( ( tmp + ( tmp >> 3 ) ) & 030707070707 ) % 63;
}

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

inline bool is_equal( f32 f0, f32 f1, const f32 eps = FLT_EPSILON )
{
    const f32 diff = ::fabs( f0 - f1 );
    f0 = ::fabs( f0 );
    f1 = ::fabs( f1 );

    const f32 largest = ( f1 > f0 ) ? f1 : f0;
    return ( diff <= largest * eps ) ? true : false;
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

inline float cubicpulse( float c, float w, float x )
{
    x = fabsf( x - c );
    if( x > w ) return 0.0f;
    x /= w;
    return 1.0f - x*x*( 3.0f - 2.0f*x );
}

inline u16 depthToBits( float depth )
{
    union { float f; unsigned i; } f2i;
    f2i.f = depth;
    unsigned b = f2i.i >> 22; // take highest 10 bits
    return (u16)b;
}

inline int moduloNegInf( int a, int b )
{
    int resultA = a - ( (int)floor( (float)a / (float)b ) * b );
    //int resultB = ( a >= 0 ) ? a % b : a - ( -iceil( ::abs( a ), b ) * b );
    //SYS_ASSERT( resultA == resultB );
    return resultA;
}

//////////////////////////////////////////////////////////////////////////
#define DECL_WRAP_INC( type_name, type, stype, bit_mask ) \
	static inline type wrap_inc_##type_name( const type val, const type min, const type max ) \
{ \
	__pragma(warning(push))	\
	__pragma(warning(disable:4146))	\
	const type result_inc = val + 1; \
	const type max_diff = max - val; \
	const type max_diff_nz = (type)( (stype)( max_diff | -max_diff ) >> bit_mask ); \
	const type max_diff_eqz = ~max_diff_nz; \
	const type result = ( result_inc & max_diff_nz ) | ( min & max_diff_eqz ); \
	\
	return (result); \
	__pragma(warning(pop))	\
}
DECL_WRAP_INC( u8, u8, i8, 7 )
DECL_WRAP_INC( u16, u16, i16, 15 )
DECL_WRAP_INC( u32, u32, i32, 31 )
DECL_WRAP_INC( u64, u64, i64, 63 )
DECL_WRAP_INC( i8, i8, i8, 7 )
DECL_WRAP_INC( i16, i16, i16, 15 )
DECL_WRAP_INC( i32, i32, i32, 31 )
DECL_WRAP_INC( i64, i64, i64, 63 )

///
///
#define DECL_WRAP_DEC( type_name, type, stype, bit_mask ) \
	static inline type wrap_dec_##type_name( const type val, const type min, const type max ) \
{ \
	__pragma(warning(push))	\
	__pragma(warning(disable:4146))	\
	const type result_dec = val - 1; \
	const type min_diff = min - val; \
	const type min_diff_nz = (type)( (stype)( min_diff | -min_diff ) >> bit_mask ); \
	const type min_diff_eqz = ~min_diff_nz; \
	const type result = ( result_dec & min_diff_nz ) | ( max & min_diff_eqz ); \
	\
	return (result); \
	__pragma(warning(pop))	\
} 
DECL_WRAP_DEC( u8, u8, i8, 7 )
DECL_WRAP_DEC( u16, u16, i16, 15 )
DECL_WRAP_DEC( u32, u32, i32, 31 )
DECL_WRAP_DEC( u64, u64, i64, 63 )
DECL_WRAP_DEC( i8, i8, i8, 7 )
DECL_WRAP_DEC( i16, i16, i16, 15 )
DECL_WRAP_DEC( i32, i32, i32, 31 )
DECL_WRAP_DEC( i64, i64, i64, 63 )
//////////////////////////////////////////////////////////////////////////


