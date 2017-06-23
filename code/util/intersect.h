#pragma once

#include "vectormath/vectormath.h"
#include "common.h"

inline bool IntersectRayAABB( const Vector3F& start, const Vector3F& dir, const Vector3F& min, const Vector3F& max, float& t, Vector3F* normal )
{
    //! calculate candidate plane on each axis
    float tx = -1.0f, ty = -1.0f, tz = -1.0f;
    bool inside = true;

    //! use unrolled loops

    //! x
    if( start.x < min.x )
    {
        if( dir.x != 0.0f )
            tx = ( min.x - start.x ) / dir.x;
        inside = false;
    }
    else if( start.x > max.x )
    {
        if( dir.x != 0.0f )
            tx = ( max.x - start.x ) / dir.x;
        inside = false;
    }

    //! y
    if( start.y < min.y )
    {
        if( dir.y != 0.0f )
            ty = ( min.y - start.y ) / dir.y;
        inside = false;
    }
    else if( start.y > max.y )
    {
        if( dir.y != 0.0f )
            ty = ( max.y - start.y ) / dir.y;
        inside = false;
    }

    //! z
    if( start.z < min.z )
    {
        if( dir.z != 0.0f )
            tz = ( min.z - start.z ) / dir.z;
        inside = false;
    }
    else if( start.z > max.z )
    {
        if( dir.z != 0.0f )
            tz = ( max.z - start.z ) / dir.z;
        inside = false;
    }

    //! if point inside all planes
    if( inside )
    {
        t = 0.0f;
        return true;
    }

    //! we now have t values for each of possible intersection planes
    //! find the maximum to get the intersection point
    float tmax = tx;
    int taxis = 0;

    if( ty > tmax )
    {
        tmax = ty;
        taxis = 1;
    }
    if( tz > tmax )
    {
        tmax = tz;
        taxis = 2;
    }

    if( tmax < 0.0f )
        return false;

    //! check that the intersection point lies on the plane we picked
    //! we don't test the axis of closest intersection for precision reasons

    //! no eps for now
    float eps = 0.0f;

    Vector3F hit = start + dir*tmax;

    if( ( hit.x < min.x - eps || hit.x > max.x + eps ) && taxis != 0 )
        return false;
    if( ( hit.y < min.y - eps || hit.y > max.y + eps ) && taxis != 1 )
        return false;
    if( ( hit.z < min.z - eps || hit.z > max.z + eps ) && taxis != 2 )
        return false;

    //! output results
    t = tmax;

    return true;
}

inline bool IntersectPlaneAABB( const Vector4F& plane, const Vector3F& center, const Vector3F& extents )
{
    float radius = ::fabsf( extents.x*plane.x ) + fabsf( extents.y*plane.y ) + fabsf( extents.z*plane.z );
    float delta = dot( center, plane.getXYZ() ) + plane.w;

    return ::fabsf( delta ) <= radius;
}


//////////////////////////////////////////////////////////////////////////
// Moller and Trumbore's method
inline bool IntersectRayTriTwoSided( const Vector3F& p, const Vector3F& dir, const Vector3F& a, const Vector3F& b, const Vector3F& c, float& t, float& u, float& v, float& w, float& sign )//Vector3F* normal)
{
    Vector3F ab = b - a;
    Vector3F ac = c - a;
    Vector3F n = cross( ab, ac );

    float d = dot( -dir, n );
    float ood = 1.0f / d; // No need to check for division by zero here as infinity aritmetic will save us...
    Vector3F ap = p - a;

    t = dot( ap, n ) * ood;
    if( t < 0.0f )
        return false;

    Vector3F e = cross( -dir, ap );
    v = dot( ac, e ) * ood;
    if( v < 0.0f || v > 1.0f ) // ...here...
        return false;
    w = -dot( ab, e ) * ood;
    if( w < 0.0f || v + w > 1.0f ) // ...and here
        return false;

    u = 1.0f - v - w;
    //if (normal)
    //*normal = n;
    sign = d;

    return true;
}