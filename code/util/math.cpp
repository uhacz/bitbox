#include "math.h"
#include "common.h"

namespace
{
    float abs_column_sum( const Matrix3& a, int i )
    {
        return sum( absPerElem( a[i] ) ).getAsFloat();
        //return abs( a[0][i] ) + abs( a[1][i] ) + abs( a[2][i] );
    }

    float abs_row_sum( const Matrix3& a, int i )
    {
        return sum( absPerElem( Vector3( a[0][i], a[1][i], a[2][i] ) ) ).getAsFloat();
        //return btFabs( a[i][0] ) + btFabs( a[i][1] ) + btFabs( a[i][2] );
    }

    float p1_norm( const Matrix3& a )
    {
        const float sum0 = abs_column_sum( a, 0 );
        const float sum1 = abs_column_sum( a, 1 );
        const float sum2 = abs_column_sum( a, 2 );
        return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
    }

    float pinf_norm( const Matrix3& a )
    {
        const float sum0 = abs_row_sum( a, 0 );
        const float sum1 = abs_row_sum( a, 1 );
        const float sum2 = abs_row_sum( a, 2 );
        return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
    }
}///
unsigned int bxPolarDecomposition( const Matrix3& a, Matrix3& u, Matrix3& h, unsigned maxIterations )
{
    const float tolerance = BX_POLAR_DECOMPOSITION_DEFAULT_TOLERANCE;
    // Use the 'u' and 'h' matrices for intermediate calculations
    u = a;
    h = inverse( a );

    for ( unsigned int i = 0; i < maxIterations; ++i )
    {
        const float h_1 = p1_norm( h );
        const float h_inf = pinf_norm( h );
        const float u_1 = p1_norm( u );
        const float u_inf = pinf_norm( u );

        const float h_norm = h_1 * h_inf;
        const float u_norm = u_1 * u_inf;

        // The matrix is effectively singular so we cannot invert it
        if ( (h_norm < FLT_EPSILON) || (u_norm < FLT_EPSILON) )
            break;

        const float gamma = ::powf( h_norm / u_norm, 0.25f );
        const float inv_gamma = float( 1.0 ) / gamma;

        // Determine the delta to 'u'
        const Matrix3 delta = (u * (gamma - float( 2.0 )) + transpose( h ) * inv_gamma) * float( 0.5 );

        // Update the matrices
        u += delta;
        h = inverse( u );

        // Check for convergence
        if ( p1_norm( delta ) <= tolerance * u_1 )
        {
            h = transpose( u ) * a;
            h = (h + transpose( h )) * 0.5;
            return i;
        }
    }

    // The algorithm has failed to converge to the specified tolerance, but we
    // want to make sure that the matrices returned are in the right form.
    h = transpose( u ) * a;
    h = (h + transpose( h )) * 0.5;

    return maxIterations;
}


float abs_column_sum( const Matrix3F& a, int i )
{
    return sum( absPerElem( a[i] ) );
}

float abs_row_sum( const Matrix3F& a, int i )
{
    return sum( absPerElem( Vector3F( a[0][i], a[1][i], a[2][i] ) ) );
}

float p1_norm( const Matrix3F& a )
{
    const float sum0 = abs_column_sum( a, 0 );
    const float sum1 = abs_column_sum( a, 1 );
    const float sum2 = abs_column_sum( a, 2 );
    return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
}

float pinf_norm( const Matrix3F& a )
{
    const float sum0 = abs_row_sum( a, 0 );
    const float sum1 = abs_row_sum( a, 1 );
    const float sum2 = abs_row_sum( a, 2 );
    return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
}
unsigned int PolarDecomposition( const Matrix3F& a, Matrix3F& u, Matrix3F& h, unsigned maxIterations /*= BX_POLAR_DECOMPOSITION_DEFAULT_MAX_ITERATIONS */ )
{
    const float tolerance = BX_POLAR_DECOMPOSITION_DEFAULT_TOLERANCE;
    // Use the 'u' and 'h' matrices for intermediate calculations
    u = a;
    h = inverse( a );

    for( unsigned int i = 0; i < maxIterations; ++i )
    {
        const float h_1 = p1_norm( h );
        const float h_inf = pinf_norm( h );
        const float u_1 = p1_norm( u );
        const float u_inf = pinf_norm( u );

        const float h_norm = h_1 * h_inf;
        const float u_norm = u_1 * u_inf;

        // The matrix is effectively singular so we cannot invert it
        if( ( h_norm < FLT_EPSILON ) || ( u_norm < FLT_EPSILON ) )
            break;

        const float gamma = ::powf( h_norm / u_norm, 0.25f );
        const float inv_gamma = float( 1.0 ) / gamma;

        // Determine the delta to 'u'
        const Matrix3F delta = ( u * ( gamma - float( 2.0 ) ) + transpose( h ) * inv_gamma ) * float( 0.5 );

        // Update the matrices
        u += delta;
        h = inverse( u );

        // Check for convergence
        if( p1_norm( delta ) <= tolerance * u_1 )
        {
            h = transpose( u ) * a;
            h = ( h + transpose( h ) ) * 0.5;
            return i;
        }
    }

    // The algorithm has failed to converge to the specified tolerance, but we
    // want to make sure that the matrices returned are in the right form.
    h = transpose( u ) * a;
    h = ( h + transpose( h ) ) * 0.5;

    return maxIterations;
}

Vector4 toAxisAngle( const Quat& q )
{
    const float quatEpsilon( 1.0e-8f );
    const float s2 = lengthSqr( q.getXYZ() ).getAsFloat(); // x*x + y*y + z*z;
    if( s2 < quatEpsilon*quatEpsilon )
    {
        return Vector4( 1.0f, 0.f, 0.f, 0.f );
    }

    const float s = ::sqrtf( s2 );
    const Vector3 axis = q.getXYZ() * s;
    const float w = q.getW().getAsFloat();
    const float angle = (::fabsf( w ) < quatEpsilon) ? PI : ::atan2f( s2*s, w ) * 2.f;
    return Vector4( axis, angle );
    
    //if ( s2 < quatEpsilon*quatEpsilon )  // can't extract a sensible axis
    //{
    //    
    //    //angle = 0.0f;
    //    //axis = PxVec3( 1.0f, 0.0f, 0.0f );
    //}
    //else
    //{
    //    const float s = PxRecipSqrt( s2 );
    //    axis = PxVec3( x, y, z ) * s;
    //    angle = PxAbs( w ) < quatEpsilon ? PxPi : PxAtan2( s2*s, w ) * 2.0f;
    //}
}
