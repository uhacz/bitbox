/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#define __SSE__
#define _VECTORMATH_NO_SCALAR_CAST 1
#include "SSE/vectormath_aos.h"
using namespace Vectormath;
using namespace Vectormath::Aos;

#include <float.h>
#define zeroVec floatInVec(0.f)
#define oneVec floatInVec(1.f)
#define halfVec floatInVec(0.5f)
#define twoVec floatInVec(2.f)
#define threeVec floatInVec(3.f)
#define piVec floatInVec( (float)3.14159265358979323846f )
#define twopiVec floatInVec( (float)6.28318530717958647692f )
#define halfpiVec floatInVec( (float)1.57079632679489661923f )
#define fltEpsVec floatInVec( FLT_EPSILON )

#define vec_floor(v) _mm_floor_ps( v )

struct Vector2
{
	Vector2() {}
	Vector2( float x_, float y_ )
		: x(x_), y(y_) {}
	
	float x;
	float y;
};

union SSEScalar
{
    SSEScalar( const __m128 v )
    {
        as_vec128f = v;
    }
       
    __m128 as_vec128f;
    __m128i as_vec128i;

    float    as_float[4];
    int      as_int[4];
    unsigned as_uint[4];

    struct  
    {
        float x, y, z, w;
    };
    struct  
    {
        int ix, iy, iz, iw;
    };

    struct  
    {
        unsigned ux, uy, uz, uw;
    };
};

inline __m128 xyz_to_m128( const float* input )
{
    return _mm_setr_ps( input[0], input[1], input[2], 0.f );
}
inline __m128 xyzw_to_m128( const float* input )
{
    return _mm_setr_ps( input[0], input[1], input[2], input[3] );
}

inline void m128_to_xyz( float* output, const __m128 input )
{
    const float* tmp = (const float*)&input;
    output[0] = tmp[0];
    output[1] = tmp[1];
    output[2] = tmp[2];
}
inline void m128_to_xyzw( float* output, const __m128 input )
{
    const float* tmp = (const float*)&input;
    output[0] = tmp[0];
    output[1] = tmp[1];
    output[2] = tmp[2];
    output[3] = tmp[3];
}

inline const Matrix4 orthographicMatrix( float left, float right, float bottom, float top, float zNear, float zFar )
{
	float tx = - (right + left)/(right - left);
	float ty = - (top + bottom)/(top - bottom);
	//float tz = - (zFar + zNear)/(zFar - zNear);

	return Matrix4(
				  Vector4( 2.f/(right-left), 0.f, 0.f, tx )
				, Vector4( 0.f, 2.f/(top-bottom), ty, 0.f )
				, Vector4( 0.f, 0.f, -2.f/(zFar - zNear), 0.f )
				, Vector4( 0.f, 0.f, 0.f, 1.f ) );
}

inline const floatInVec minf4( const floatInVec &v0, const floatInVec &v1 )
{
	return floatInVec( _mm_min_ps( v0.get128(), v1.get128() ) );
}

inline const floatInVec maxf4( const floatInVec &v0, const floatInVec &v1 )
{
	return floatInVec( _mm_max_ps( v0.get128(), v1.get128() ) );
}

inline const Vector4 maddv4( const Vector4& a, const Vector4& b, const Vector4& c )
{
	return Vector4(		_mm_add_ps( c.get128(), _mm_mul_ps(a.get128(), b.get128()) )	);
}

inline const Vector3 maddv3( const Vector3& a, const Vector3& b, const Vector3& c )
{
	return Vector3(		_mm_add_ps( c.get128(), _mm_mul_ps(a.get128(), b.get128()) )	);
}
inline floatInVec clampf4( const floatInVec& x, const floatInVec& a, const floatInVec& b )
{
	return floatInVec( maxf4(a, minf4(b,x) ) );
}


inline Vector3 clampv( const Vector3& x, const Vector3& a, const Vector3& b )
{
	return maxPerElem( a, minPerElem(b,x) );
}


inline Vector4 clampv( const Vector4& x, const Vector4& a, const Vector4& b )
{
	return maxPerElem( a, minPerElem(b,x) );
}
inline const floatInVec absf4( const floatInVec& v )
{
    return fabsf4( v.get128() );
}

inline floatInVec recipf( const floatInVec& x )
{
    return recipf4( x.get128() );
}

inline floatInVec sinef4( const floatInVec& x )
{
    return sinf4( x.get128() );
}
inline Vector3 sinv3( const Vector3& x )
{
    return Vector3( sinf4( x.get128() ) );
}
inline Vector4 sinv4( const Vector4& x )
{
    return Vector4( sinf4( x.get128() ) );
}

inline const Vector3 normalizeSafe( const Vector3& v, const floatInVec& eps = floatInVec( 0.0001f ) )
{
    return select( Vector3(0.f), normalize(v), lengthSqr( v ) >= eps );
}
inline const Vector3 normalizeSafeApprox( const Vector3& v, const floatInVec& eps = floatInVec( 0.0001f ) )
{
    return select( Vector3(0.f), normalizeApprox(v), lengthSqr( v ) >= eps );
}

inline floatInVec linearstepf4( const floatInVec& min, const floatInVec& max, const floatInVec& t )
{
    return ( clampf4( t, min, max ) - min ) / ( max - min );
}

inline floatInVec smoothstepf4( const floatInVec& min, const floatInVec& max, const floatInVec& t )
{
    const floatInVec x = ( clampf4( t, min ,max ) - min ) * recipf( max - min );
    return x*x*( threeVec - twoVec * x );
}

inline vec_float4 lerpf4( const vec_float4& t, const vec_float4& a, const vec_float4& b )
{
    return vec_add( a, vec_mul( t, vec_sub( b, a ) ) );
}

inline const Vector3 mulAsVec4( const Matrix4& mat, const Vector3& vec )
{
    const __m128 vec128 = vec.get128();

    return Vector3( (
        _mm_add_ps(
        _mm_add_ps(_mm_mul_ps((mat.getCol0().get128()), _mm_shuffle_ps(vec128, vec128, _MM_SHUFFLE(0,0,0,0))), _mm_mul_ps((mat.getCol1().get128()), _mm_shuffle_ps(vec128, vec128, _MM_SHUFFLE(1,1,1,1)))),
        _mm_add_ps(_mm_mul_ps((mat.getCol2().get128()), _mm_shuffle_ps(vec128, vec128, _MM_SHUFFLE(2,2,2,2))), _mm_mul_ps((mat.getCol3().get128()), _mm_setr_ps(1.0f,1.0f,1.0f,1.0f))))
        )
        );
}

inline Quat fastQuatY( const floatInVec& sine, const floatInVec& cosine )
{
    __m128 res;
    const __m128 s = sine.get128();
    const __m128 c = cosine.get128();
	VM_ATTRIBUTE_ALIGN16 unsigned int ysw[4] = {0, 0xffffffff, 0, 0};
	VM_ATTRIBUTE_ALIGN16 unsigned int wsw[4] = {0, 0, 0, 0xffffffff};
    res = vec_sel( _mm_setzero_ps(), s, ysw );
    res = vec_sel( res, c, wsw );
    return Quat( res );
}

inline Vector3 fastRotate( const Quat& q, const Vector3& v )
{
    const Vector3 t = twoVec * cross( q.getXYZ(), v );
    return v + q.getW() * t + cross( q.getXYZ(), t );
}
inline Vector3 fastTransform( const Quat& rot, const Vector3& trans, const Vector3& point )
{
    return trans + fastRotate( rot, point );
}

//////////////////////////////////////////////////////////////////////////
/// compute pow( x, y ) for range [0, 1] using SSE
/// x_vec must be in [0,1] range !!!
// source: https://gist.github.com/Novum/1200562
inline floatInVec fastPseudoPow_01( const floatInVec& x_vec, const floatInVec& y_vec )
{
    const __m128 fourOne = _mm_set1_ps(1.0f);
    const __m128 fourHalf = _mm_set1_ps(0.5f);

    const __m128 y = y_vec.get128();
    const __m128 x = x_vec.get128();

    __m128 a = _mm_sub_ps(fourOne, y);
    __m128 b = _mm_sub_ps(x, fourOne);
    __m128 aSq = _mm_mul_ps(a, a);
    __m128 bSq = _mm_mul_ps(b, b);
    __m128 c = _mm_mul_ps(fourHalf, bSq);
    __m128 d = _mm_sub_ps(b, c);
    __m128 dSq = _mm_mul_ps(d, d);
    __m128 e = _mm_mul_ps(aSq, dSq);
    __m128 f = _mm_mul_ps(a, d);
    __m128 g = _mm_mul_ps(fourHalf, e);
    __m128 h = _mm_add_ps(fourOne, f);
    __m128 i = _mm_add_ps(h, g);	
    __m128 iRcp = _mm_rcp_ps(i);
    __m128 result = _mm_mul_ps(x, iRcp);

    return floatInVec( result );
}

/// implementation based on http://www.research.scea.com/gdc2003/fast-math-functions_p2.pdf
/// x_vec must be in [0,1] range !!!
inline floatInVec fastPow_01Approx( const floatInVec& x_vec, const floatInVec& y_vec )
{
    const floatInVec& a = x_vec;
    const floatInVec b = y_vec * halfVec;
    return a * recipf( b - a*b + a );
}
