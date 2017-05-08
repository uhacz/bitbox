#pragma once

#include "..\common.h"

template< typename T >
struct TVector2
{
    union
    {
        T xy[2];
        struct { T x, y; };
    };

    TVector2() {}
    TVector2( T v ) : x( v ), y( v ) {}
    TVector2( T v0, T v1 ) : x( v0 ), y( v1 ) {}

    TVector2& operator = ( const TVector2& v )
    {
        x = v.x; y = v.y;
        return *this;
    }
    TVector2 operator + ( const TVector2& v ) const { return TVector2( x + v.x, y + v.y ); }
    TVector2 operator - ( const TVector2& v ) const { return TVector2( x - v.x, y - v.y ); }
    TVector2 operator * ( float v )           const { return TVector2( (T)( x * v ), (T)( y * v ) ); }
    TVector2 operator / ( float v )           const { return TVector2( (T)( x / v ), (T)( y / v ) ); }
    TVector2 operator - ()                    const { return TVector2( -x, -y ); }
};
template< typename T > inline TVector2<T> mulPerElem( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( a.x * b.x, a.y * b.y ); }
template< typename T > inline TVector2<T> divPerElem( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( a.x / b.x, a.y / b.y ); }
template< typename T > inline TVector2<T> minPerElem( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( minOfPair( a.x, b.x ), minOfPair( a.y, b.y ) ); }
template< typename T > inline TVector2<T> maxPerElem( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( maxOfPair( a.x, b.x ), maxOfPair( a.y, b.y ) ); }
template< typename T > inline TVector2<T> absPerElem( const TVector2<T>& a ) { return TVector2<T>( ::abs( a.x ), ::abs( a.y ) ); }

template< typename T > inline TVector2<T> toVector2xy( const Vector3F& v ) { return TVector2<T>( (T)v.x, (T)v.y ); }
template< typename T > inline TVector2<T> toVector2xz( const Vector3F& v ) { return TVector2<T>( (T)v.x, (T)v.z ); }
template< typename T > inline TVector2<T> toVector2yz( const Vector3F& v ) { return TVector2<T>( (T)v.y, (T)v.z ); }
template< typename T > inline Vector3F toVector3Fxy( const TVector2<T>& v ) { return Vector3F( (f32)v.x, (f32)v.y, 0.f ); }
template< typename T > inline Vector3F toVector3Fxz( const TVector2<T>& v ) { return Vector3F( (f32)v.x, 0.f, (f32)v.y ); }
template< typename T > inline Vector3F toVector3Fyz( const TVector2<T>& v ) { return Vector3F( 0.f, (f32)v.x, (f32)v.y ); }

typedef TVector2<i32> Vector2I;
typedef TVector2<u32> Vector2UI;
