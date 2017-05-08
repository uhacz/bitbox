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


#include "scalar/vectormath_aos.h"
namespace smath = VectormathScalar::Aos;
using Vector3F = smath::Vector3;
using Point3F  = smath::Point3;
using Vector4F = smath::Vector4;
using QuatF    = smath::Quat;
using Matrix3F = smath::Matrix3;
using Matrix4F = smath::Matrix4;

#include "vector2.h"

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
#define vec_cmplt(a,b) _mm_cmplt_ps( a, b )
#define vec_cmple( a, b ) _mm_cmple_ps( a, b )
#define vec_cmpge( a, b ) _mm_cmpge_ps( a, b )
#define vec_div(a,b) _mm_div_ps( a, b )
#define vec_min( a, b ) _mm_min_ps( a, b )
#define vec_max( a, b ) _mm_max_ps( a, b )

inline vec_float4 copysignf4( vec_float4 x, vec_float4 y ) { return vec_sel( x, y, ( 0x80000000 ) ); }
inline vec_float4 recipi_f4( vec_float4 vec, vec_float4 estimate )
{
    return vec_nmsub( estimate, _mm_mul_ps( vec, estimate ), _mm_add_ps( estimate, estimate ) );
}

static __forceinline __m128 recipf4_newtonrapson( const __m128 x )
{
    __m128 estimate = _mm_rcp_ps( x );
    estimate = recipi_f4( x, estimate );
    estimate = recipi_f4( x, estimate );
    return estimate;
}

#include "SSE/vectormath_soa.h"

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
    SSEScalar( const __m128i v )
    {
        as_vec128i = v;
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

inline const Vector3 normalizeSafe( const Vector3& v, const floatInVec& eps = floatInVec( FLT_EPSILON ) )
{
    return select( Vector3(0.f), normalize(v), lengthSqr( v ) >= eps );
}
inline const Vector3 normalizeSafeApprox( const Vector3& v, const floatInVec& eps = floatInVec( FLT_EPSILON ) )
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
inline Vector3 fastRotateInv( const Quat& q, const Vector3& v )
{
    const Vector3 t = twoVec * cross( q.getXYZ(), v );
    return v - q.getW() * t + cross( q.getXYZ(), t );
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

inline Vector4 makePlane( const Vector3& planeNormal, const Vector3& pointOnPlane )
{
    return Vector4( planeNormal,  -dot( planeNormal, pointOnPlane ) );	
}
inline Vector4F makePlane( const Vector3F& planeNormal, const Vector3F& pointOnPlane )
{
    return Vector4F( planeNormal, -dot( planeNormal, pointOnPlane ) );
}


inline Vector3 projectPointOnPlane( const Vector3& point, const Vector4& plane )
{
    const Vector3& n = plane.getXYZ();
    const Vector3& Q = point;

    const Vector3 Qp = Q - (dot(Q,n) + plane.getW()) * n;
    return Qp;
}

inline Vector3 projectVectorOnPlane( const Vector3& vec, const Vector4& plane )
{
    const Vector3& n = plane.getXYZ();
    const Vector3& V = vec;
    const Vector3 W = V - dot(V,n) * n;
    return W;
}

inline Vector3F projectPointOnPlane( const Vector3F& point, const Vector4F& plane )
{
    const Vector3F& n = plane.getXYZ();
    const Vector3F& Q = point;

    const Vector3F Qp = Q - ( dot( Q, n ) + plane.getW() ) * n;
    return Qp;
}

inline Vector3F projectVectorOnPlane( const Vector3F& vec, const Vector3F& planeNormal )
{
    const Vector3F& n = planeNormal;
    const Vector3F& V = vec;
    const Vector3F W = V - dot( V, n ) * n;
    return W;
}
inline Vector3F projectVectorOnPlane( const Vector3F& vec, const Vector4F& plane )
{
    return projectVectorOnPlane( vec, plane.getXYZ() );
}


inline Matrix3 computeBasis( const Vector3& n )
{
    if( n.getZ().getAsFloat() < -0.9999999f )
    {
        return Matrix3( Vector3( 0.f, -1.f, 0.f ), n, Vector3( -1.f, 0.f, 0.f ) );
    }

    const floatInVec a = oneVec / ( oneVec + n.getZ() );
    const floatInVec b = -n.getX()*n.getY()*a;
    const Vector3 x = Vector3( oneVec - n.getX()*n.getX()*a, b, -n.getX() );
    const Vector3 z = Vector3( b, oneVec - n.getY()*n.getY()*a, -n.getY() );
    return Matrix3( x, n, z );
}
static inline Quat shortestRotation( const Vector3& v0, const Vector3& v1 )
{
    const float d = dot( v0, v1 ).getAsFloat();
    const Vector3 c = cross( v0, v1 );
    const SSEScalar s( v0.get128() );

    Quat q = d > -1.f ? Quat( c, 1.f + d )
        : fabs( s.x ) < 0.1f ? Quat( 0.0f, s.z, -s.y, 0.0f ) : Quat( s.y, -s.x, 0.0f, 0.0f );

    return normalize( q );
}

inline float computeAngle( const Vector3& v0, const Vector3& v1 )
{
    const float cosine = dot( v0, v1 ).getAsFloat();
    const float sine = length( cross( v0, v1 ) ).getAsFloat();

    return ::atan2( sine, cosine );
}

struct TransformTQ
{
    Quat q;
    Vector3 t;

    TransformTQ() {}
    
    TransformTQ( const Quat& r )
        : q(r), t(0.f) {}
    
    TransformTQ( const Vector3& v )
        : q( Quat::identity() ), t( v ) {}
    
    TransformTQ( const Quat& r, const Vector3& v )
        : q( r ), t( v ) {}

    TransformTQ( const Matrix4& m )
        : q( m.getUpper3x3() ), t( m.getTranslation() ) {}

    inline static TransformTQ identity() { return TransformTQ( Quat::identity(), Vector3( 0.f ) ); }
};

inline Vector3 transform( const TransformTQ& tq, const Vector3& point )
{
    return fastTransform( tq.q, tq.t, point );
}
inline TransformTQ transform( const TransformTQ& a, const TransformTQ& b )
{
	return TransformTQ( a.q * b.q, fastTransform( a.q, a.t, b.t ) );
}
inline TransformTQ transformInv( const TransformTQ& a, const TransformTQ& b )
{
	const Quat qconj = conj( a.q );
	return TransformTQ( qconj * b.q, rotate( qconj, b.t - a.t ) );
}


inline void addStoreXYZArray( const Vector3 &vec0, const Vector3 &vec1, const Vector3 &vec2, const Vector3 &vec3, __m128 * threeQuads )
{
	__m128 xxxx = _mm_shuffle_ps( vec1.get128(), vec1.get128(), _MM_SHUFFLE( 0, 0, 0, 0 ) );
	__m128 zzzz = _mm_shuffle_ps( vec2.get128(), vec2.get128(), _MM_SHUFFLE( 2, 2, 2, 2 ) );
	VM_ATTRIBUTE_ALIGN16 unsigned int xsw[4] = { 0, 0, 0, 0xffffffff };
	VM_ATTRIBUTE_ALIGN16 unsigned int zsw[4] = { 0xffffffff, 0, 0, 0 };
	threeQuads[0] = _mm_add_ps( threeQuads[0], vec_sel( vec0.get128(), xxxx, xsw ) );
	threeQuads[1] = _mm_add_ps( threeQuads[1], _mm_shuffle_ps( vec1.get128(), vec2.get128(), _MM_SHUFFLE( 1, 0, 2, 1 ) ) );
	threeQuads[2] = _mm_add_ps( threeQuads[2], vec_sel( _mm_shuffle_ps( vec3.get128(), vec3.get128(), _MM_SHUFFLE( 2, 1, 0, 3 ) ), zzzz, zsw ) );
}

//////////////////////////////////////////////////////////////////////////
inline Vector3 toVector3( const Vector3F& v ) { return Vector3( v.x, v.y, v.z ); }
inline Vector4 toVector4( const Vector4F& v ) { return Vector4( v.x, v.y, v.z, v.w ); }
inline Quat    toQuat   ( const QuatF& q )    { return Quat( q.x, q.y, q.z, q.w ); }
inline Matrix3 toMatrix3( const Matrix3F& m )
{
    return Matrix3(
        toVector3( m.getCol0() ),
        toVector3( m.getCol1() ),
        toVector3( m.getCol2() )
        );
}

inline Matrix4 toMatrix4( const Matrix4F& m )
{
    return Matrix4(
        toVector4( m.getCol0() ),
        toVector4( m.getCol1() ),
        toVector4( m.getCol2() ),
        toVector4( m.getCol3() )
        );
}
//////////////////////////////////////////////////////////////////////////
inline Vector3F toVector3F( const Vector3& v ) 
{ 
    Vector3F result;
    m128_to_xyz( result.xyz, v.get128() );
    return result;
}
inline Vector4F toVector4F( const Vector4& v )
{
    Vector4F result;
    m128_to_xyzw( result.xyzw, v.get128() );
    return result;
}
inline QuatF    toQuatF( const Quat& q )
{
    QuatF result;
    m128_to_xyzw( result.xyzw, q.get128() );
    return result;
}
inline Matrix3F toMatrix3F( const Matrix3& m )
{
    return Matrix3F(
        toVector3F( m.getCol0() ),
        toVector3F( m.getCol1() ),
        toVector3F( m.getCol2() )
        );
}

inline Matrix4F toMatrix4F( const Matrix4& m )
{
    return Matrix4F(
        toVector4F( m.getCol0() ),
        toVector4F( m.getCol1() ),
        toVector4F( m.getCol2() ),
        toVector4F( m.getCol3() )
        );
}
