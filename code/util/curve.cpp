#include "curve.h"
#include "memory.h"
#include <memory.h>
#include "common.h"

namespace bx
{
    namespace curve_internal
    {
        inline u32 find_knot( const f32* knots, u32 n, float t )
        {
            if( !n )
                return UINT32_MAX;

            u32 i = 0;
            do
            {
                if( knots[i] >= t )
                    break;
            } while( ++i < n );

            return i;
        }
    }

    void curve::allocate( Curve3D& cv, u32 size )
    {
        u32 mem_size = 0;
        mem_size += size * sizeof( Vector3 );
        mem_size += size * sizeof( f32 );

        u8* mem = (u8*)BX_MALLOC( bxDefaultAllocator(), mem_size, 16 );
        memset( mem, 0, mem_size );

        cv._points = (Vector3*)mem;
        cv._knots = (f32*)( cv._points + size );
        cv._capacity = size;
        cv._size = 0;
    }

    void curve::deallocate( Curve3D& cv )
    {
        BX_FREE0( bxDefaultAllocator(), cv._points );
        cv = Curve3D();
    }

    void curve::clear( Curve3D& cv )
    {
        cv._size = 0;
    }

    u32 curve::push_back( Curve3D& cv, const Vector3& point, f32 t )
    {
        u32 index = cv._size;
        if( index >= cv._capacity )
            return UINT32_MAX;

        cv._points[index] = point;
        cv._knots[index] = t;
        ++cv._size;
        return index;
    }

    const Vector3 curve::evaluate_catmullrom( const Curve3D& cv, f32 t )
    {
        if( !cv._size )
            return Vector3( 0.f );

        static const Vector4 coeffs[] =
        {
            Vector4( -1.f, 2.f, -1.f, 0.f ),
            Vector4( 3.f, -5.f, 0.f, 2.f ),
            Vector4( -3.f, 4.f, 1.f, 0.f ),
            Vector4( 1.f, -1.f, 0.f, 0.f ),
        };

        const u32 ki = curve_internal::find_knot( cv._knots, cv._size, t );
        const u32 kim1 = ( ki > 0 ) ? ki - 1 : ki;
        const u32 kim2 = ( ki > 1 ) ? ki - 2 : kim1;
        const u32 kip1 = ( ki < ( cv._size - 1 ) ) ? ki + 1 : ki;

        const float prev_k = cv._knots[kim1];
        const float k = cv._knots[ki];
        const float t01 = ( is_equal( k, prev_k ) ) ? 0.f : ( t - prev_k ) / ( k - prev_k );

        const floatInVec half( 0.5f );
        const floatInVec one( half + half );
        const floatInVec t01v( t01 );
        const floatInVec t012v = t01v*t01v;
        const floatInVec t013v = t01v * t012v;
        const Vector4 hv4( t013v, t012v, t01v, one );

        const Vector3& pm2 = cv._points[kim2];
        const Vector3& pm1 = cv._points[kim1];
        const Vector3& p = cv._points[ki];
        const Vector3& pp1 = cv._points[kip1];

        const Vector4 p0( pm2.getX(), pm1.getX(), p.getX(), pp1.getX() );
        const Vector4 p1( pm2.getY(), pm1.getY(), p.getY(), pp1.getY() );
        const Vector4 p2( pm2.getZ(), pm1.getZ(), p.getZ(), pp1.getZ() );

        const floatInVec t0 = dot( hv4, coeffs[0] );
        const floatInVec t1 = dot( hv4, coeffs[1] );
        const floatInVec t2 = dot( hv4, coeffs[2] );
        const floatInVec t3 = dot( hv4, coeffs[3] );

        const Vector4 tv = Vector4( t0, t1, t2, t3 ) * half;
        const Vector3 result( dot( tv, p0 ), dot( tv, p1 ), dot( tv, p2 ) );
        return result;
    }

    const Matrix4 curve::compute_pose_catmullrom( const Curve3D& cv, f32 t, f32 delta /*= 0.0001f */ )
    {
        const Vector3 p0 = evaluate_catmullrom( cv, t );
        const Vector3 p1 = evaluate_catmullrom( cv, t + delta );

        return Matrix4::identity();
    }
    
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    void curve::allocate( Curve1D& cv, u32 size )
    {
        u32 mem_size = 0;
        mem_size += size * sizeof( f32 );
        mem_size += size * sizeof( f32 );

        u8* mem = (u8*)BX_MALLOC( bxDefaultAllocator(), mem_size, 4 );
        memset( mem, 0, mem_size );

        cv._points = (f32*)mem;
        cv._knots = (f32*)( cv._points + size );
        cv._capacity = size;
        cv._size = 0;
    }
    void curve::deallocate( Curve1D& cv )
    {
        BX_FREE0( bxDefaultAllocator(), cv._points );
        cv = {};
    }
    void curve::clear( Curve1D& cv )
    {
        cv._size = 0;
    }
    u32 curve::push_back( Curve1D& cv, f32 point, f32 t )
    {
        u32 index = cv._size;
        if( index >= cv._capacity )
            return UINT32_MAX;

        cv._points[index] = point;
        cv._knots[index] = t;
        ++cv._size;
        return index;
    }

    namespace curve_internal
    {
        static signed char coefs[16] = {
            -1, 2, -1, 0,
            3, -5, 0, 2,
            -3, 4, 1, 0,
            1, -1, 0, 0 };

        float spline( const float *key, const float* value, int num, float t )
        {
            const int size = 1;

            // find key
            int k = 0; 
            while( key[k] < t ) 
                k++;

            // interpolant
            const float h = ( t - key[k - 1] ) / ( key[k] - key[k - 1] );

            float result = 0.f;

            // add basis functions
            for( int i = 0; i < 4; i++ )
            {
                const int kn = clamp( k + i - 2, 0, num - 1 ); 
                const signed char *co = coefs + 4 * i;
                const float b = 0.5f*( ( ( co[0] * h + co[1] )*h + co[2] )*h + co[3] );
                result += b * value[kn];
            }

            return result;
        }
    }

    float curve::evaluate_catmullrom( const Curve1D& cv, f32 t )
    {
        if( t <= 0.f )
            return cv._points[0];
        if( t >= 1.f )
            return cv._points[cv._size - 1];

        return curve_internal::spline( cv._knots, cv._points, cv._size, t );
    }







}//