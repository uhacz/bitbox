#pragma once

#include <util/vectormath/vectormath.h>

// intersection routines
inline bool IntersectRaySphere( const Vector3F& sphereOrigin, float sphereRadius, const Vector3F& rayOrigin, const Vector3F& rayDir, float& t, Vector3F* hitNormal = NULL )
{
    Vector3F d( sphereOrigin - rayOrigin );
    float deltaSq = lengthSqr( d );
    float radiusSq = sphereRadius*sphereRadius;

    // if the origin is inside the sphere return no intersection
    if( deltaSq > radiusSq )
    {
        float dprojr = dot( d, rayDir );

        // if ray pointing away from sphere no intersection
        if( dprojr < 0.0f )
            return false;

        // bit of Pythagoras to get closest point on ray
        float dSq = deltaSq - dprojr*dprojr;

        if( dSq > radiusSq )
            return false;
        else
        {
            // length of the half cord
            float thc = sqrtf( radiusSq - dSq );

            // closest intersection
            t = dprojr - thc;

            // calculate normal if requested
            if( hitNormal )
                *hitNormal = normalize( ( rayOrigin + rayDir*t ) - sphereOrigin );

            return true;
        }
    }
    else
    {
        return false;
    }
}


template <typename T>
inline bool SolveQuadratic( T a, T b, T c, T& minT, T& maxT )
{
    if( a == 0.0f && b == 0.0f )
    {
        minT = maxT = 0.0f;
        return true;
    }

    T discriminant = b*b - T( 4.0 )*a*c;

    if( discriminant < 0.0f )
    {
        return false;
    }

    // numerical receipes 5.6 (this method ensures numerical accuracy is preserved)
    T t = T( -0.5 ) * ( b + Sign( b )*sqrt( discriminant ) );
    minT = t / a;
    maxT = c / t;

    if( minT > maxT )
    {
        Swap( minT, maxT );
    }

    return true;
}

// alternative ray sphere intersect, returns closest and furthest t values
inline bool IntersectRaySphere( const Vector3F& sphereOrigin, float sphereRadius, const Vector3F& rayOrigin, const Vector3F& rayDir, float& minT, float &maxT, Vector3F* hitNormal = NULL )
{
    Vector3F q = rayOrigin - sphereOrigin;

    float a = 1.0f;
    float b = 2.0f*dot( q, rayDir );
    float c = dot( q, q ) - ( sphereRadius*sphereRadius );

    bool r = SolveQuadratic( a, b, c, minT, maxT );

    if( minT < 0.0 )
        minT = 0.0f;

    // calculate the normal of the closest hit
    if( hitNormal && r )
    {
        *hitNormal = normalize( ( rayOrigin + rayDir*minT ) - sphereOrigin );
    }

    return r;
}

inline bool IntersectRayPlane( const Vector3F& p, const Vector3F& dir, const Vector4F& plane, float& t )
{
    float d = dot( plane.getXYZ(), dir );

    if( d == 0.0f )
    {
        return false;
    }
    else
    {
        t = -dot( plane, Vector4F( p, 1.0f ) ) / d;
    }

    return ( t > 0.0f );
}

inline bool IntersectLineSegmentPlane( const Vector3F& start, const Vector3F& end, const Vector4F& plane, Vector3F& out )
{
    Vector3F u( end - start );
    const Vector3F N = plane.getXYZ();
    float dist = -dot( N, start ) / dot( N, u );

    if( dist > 0.0f && dist < 1.0f )
    {
        out = ( start + u * dist );
        return true;
    }
    else
        return false;
}

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



// mostly taken from Real Time Collision Detection - p192
inline bool IntersectRayTri( const Vector3F& p, const Vector3F& dir, const Vector3F& a, const Vector3F& b, const Vector3F& c, float& t, float& u, float& v, float& w, Vector3F* normal )
{
    const Vector3F ab = b - a;
    const Vector3F ac = c - a;

    // calculate normal
    Vector3F n = cross( ab, ac );

    // need to solve a system of three equations to give t, u, v
    float d = dot( -dir, n );

    // if dir is parallel to triangle plane or points away from triangle 
    if( d <= 0.0f )
        return false;

    Vector3F ap = p - a;
    t = dot( ap, n );

    // ignores tris behind 
    if( t < 0.0f )
        return false;

    // compute barycentric coordinates
    Vector3F e = cross( -dir, ap );
    v = dot( ac, e );
    if( v < 0.0f || v > d ) return false;

    w = -dot( ab, e );
    if( w < 0.0f || v + w > d ) return false;

    float ood = 1.0f / d;
    t *= ood;
    v *= ood;
    w *= ood;
    u = 1.0f - v - w;

    // optionally write out normal (todo: this branch is a performance concern, should probably remove)
    if( normal )
        *normal = n;

    return true;
}

//mostly taken from Real Time Collision Detection - p192
inline bool IntersectSegmentTri( const Vector3F& p, const Vector3F& q, const Vector3F& a, const Vector3F& b, const Vector3F& c, float& t, float& u, float& v, float& w, Vector3F* normal, float expand )
{
    const Vector3F ab = b - a;
    const Vector3F ac = c - a;
    const Vector3F qp = p - q;

    // calculate normal
    Vector3F n = cross( ab, ac );

    // need to solve a system of three equations to give t, u, v
    float d = dot( qp, n );

    // if dir is parallel to triangle plane or points away from triangle 
    if( d <= 0.0f )
        return false;

    Vector3F ap = p - a;
    t = dot( ap, n );

    // ignores tris behind 
    if( t < 0.0f )
        return false;

    // ignores tris beyond segment
    if( t > d )
        return false;

    // compute barycentric coordinates
    Vector3F e = cross( qp, ap );
    v = dot( ac, e );
    if( v < 0.0f || v > d ) return false;

    w = -dot( ab, e );
    if( w < 0.0f || v + w > d ) return false;

    float ood = 1.0f / d;
    t *= ood;
    v *= ood;
    w *= ood;
    u = 1.0f - v - w;

    // optionally write out normal (todo: this branch is a performance concern, should probably remove)
    if( normal )
        *normal = n;

    return true;
}

inline float ScalarTriple( const Vector3F& a, const Vector3F& b, const Vector3F& c ) { return dot( cross( a, b ), c ); }

// intersects a line (through points p and q, against a triangle a, b, c - mostly taken from Real Time Collision Detection - p186
inline bool IntersectLineTri( const Vector3F& p, const Vector3F& q, const Vector3F& a, const Vector3F& b, const Vector3F& c )//,  float& t, float& u, float& v, float& w, Vector3F* normal, float expand)
{
    const Vector3F pq = q - p;
    const Vector3F pa = a - p;
    const Vector3F pb = b - p;
    const Vector3F pc = c - p;

    Vector3F m = cross( pq, pc );
    float u = dot( pb, m );
    if( u< 0.0f ) return false;

    float v = -dot( pa, m );
    if( v < 0.0f ) return false;

    float w = ScalarTriple( pq, pb, pa );
    if( w < 0.0f ) return false;

    return true;
}

inline bool IntersectRayAABBOmpf( const Vector3F& pos, const Vector3F& rcp_dir, const Vector3F& min, const Vector3F& max, float& t ) {

    float
        l1 = ( min.x - pos.x ) * rcp_dir.x,
        l2 = ( max.x - pos.x ) * rcp_dir.x,
        lmin = minOfPair( l1, l2 ),
        lmax = maxOfPair( l1, l2 );

    l1 = ( min.y - pos.y ) * rcp_dir.y;
    l2 = ( max.y - pos.y ) * rcp_dir.y;
    lmin = maxOfPair( minOfPair( l1, l2 ), lmin );
    lmax = minOfPair( maxOfPair( l1, l2 ), lmax );

    l1 = ( min.z - pos.z ) * rcp_dir.z;
    l2 = ( max.z - pos.z ) * rcp_dir.z;
    lmin = maxOfPair( minOfPair( l1, l2 ), lmin );
    lmax = minOfPair( maxOfPair( l1, l2 ), lmax );

    //return ((lmax > 0.f) & (lmax >= lmin));
    //return ((lmax > 0.f) & (lmax > lmin));
    bool hit = ( ( lmax >= 0.f ) & ( lmax >= lmin ) );
    if( hit )
        t = lmin;
    return hit;
}


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
