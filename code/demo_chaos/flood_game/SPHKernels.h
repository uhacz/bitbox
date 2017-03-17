#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <util\type.h>
#include <util\vectormath\vectormath.h>
#include "util\debug.h"

//#define NO_DISTANCE_TEST

namespace PBD
{
	class CubicKernel
	{
	protected:
		static f32 m_radius;
		static f32 m_k;
		static f32 m_l;
		static f32 m_W_zero;
	public:
		static f32 getRadius() { return m_radius; }
		static void setRadius(f32 val)
		{
			m_radius = val;
			static const f32 pi = static_cast<f32>(M_PI);

			const f32 h3 = m_radius*m_radius*m_radius;
			m_k = 8.0f / (pi*h3);
			m_l = 48.0f / (pi*h3);
			m_W_zero = W(Vector3(0.0, 0.0, 0.0));
		}

	public:
		//static unsigned int counter;
		static f32 W(const Vector3 r)
		{
			//counter++;
			f32 res = 0.0f;
            const f32 rl = length( r ).getAsFloat(); // r.norm();
			const f32 q = rl/m_radius;
#ifndef NO_DISTANCE_TEST
			if (q <= 1.0)
#endif
			{
				if (q <= 0.5)
				{
					const f32 q2 = q*q;
					const f32 q3 = q2*q;
					res = m_k * (6.0f*q3-6.0f*q2+1.0f);
				}
				else
				{
					res = m_k * (2.0f*pow(1.0f-q,3));
				}
			}
			return res;
		}

		static Vector3F gradW(const Vector3F r)
		{
			Vector3F res;
			const f32 rl = length( r );
			const f32 q = rl / m_radius;
#ifndef NO_DISTANCE_TEST
            if( q <= 1.0 )
            #endif
            {
                if( rl > 1.0e-6 )
                {
                    const Vector3F gradq = r * ( ( f32 ) 1.0 / ( rl*m_radius ) );
                    if( q <= 0.5 )
                    {
                        res = m_l*q*( ( f32 ) 3.0*q - ( f32 ) 2.0 )*gradq;
                    }
                    else
                    {
                        const f32 factor = 1.0f - q;
                        res = m_l*( -factor*factor )*gradq;
                    }
                    }
            }
        #ifndef NO_DISTANCE_TEST
            else
                res = Vector3F( 0.f );
#endif

			return res;
		}

		static f32 W_zero()
		{
			return m_W_zero;
		}
	};

    class Poly6Kernel
    {
    protected:
        static f32 m_radius;
        static f32 m_k;
        static f32 m_l;
        static f32 m_m;
        static f32 m_W_zero;
    public:
        static float getRadius() { return m_radius; }
        static void setRadius( float val )
        {
            m_radius = val;
            static const float pi = static_cast<float>( M_PI );
            m_k = 315.0f / ( 64.0f*pi*pow( m_radius, 9 ) );
            m_l = -945.0f / ( 32.0f*pi*pow( m_radius, 9 ) );
            m_m = m_l;
            m_W_zero = W( Vector3F(0.f) );
        }

    public:

        /**
        * W(r,h) = (315/(64 pi h^9))(h^2-|r|^2)^3
        *        = (315/(64 pi h^9))(h^2-r*r)^3
        */
        static float W( const float r )
        {
            float res = 0.0f;
            const float r2 = r*r;
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                float a = radius2 - r2;
                a *= a*a;
                res = a * m_k;
                //res = pow( radius2 - r2, 3 )*m_k;
            }
            return res;
        }

        static float W( const Vector3F &r )
        {
            float res = 0.0f;
            const float r2 = lengthSqr( r ); // .getAsFloat();
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                float a = radius2 - r2;
                a *= a*a;
                res = a * m_k;

                //res = pow( radius2 - r2, 3 )*m_k;
            }
            return res;
        }


        /**
        * grad(W(r,h)) = r(-945/(32 pi h^9))(h^2-|r|^2)^2
        *              = r(-945/(32 pi h^9))(h^2-r*r)^2
        */
        static Vector3F gradW( const Vector3F &r )
        {
            Vector3F res(0.f);
            const float r2 = lengthSqr( r ); // .getAsFloat();
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                float tmp = radius2 - r2;
                res = m_l * tmp * tmp*r;
            }
            //else
            //    res.setZero();

            return res;
        }

        /**
        * laplacian(W(r,h)) = (-945/(32 pi h^9))(h^2-|r|^2)(-7|r|^2+3h^2)
        *                   = (-945/(32 pi h^9))(h^2-r*r)(3 h^2-7 r*r)
        */
        static float laplacianW( const Vector3F &r )
        {
            float res;
            const float r2 = lengthSqr( r ); // .getAsFloat();
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                float tmp = radius2 - r2;
                float tmp2 = 3 * radius2 - 7 * r2;
                res = m_m * tmp  * tmp2;
            }
            else
                res = ( float ) 0.;

            return res;
        }

        static float W_zero()
        {
            return m_W_zero;
        }
    };

    /** \brief Spiky kernel.
    */
    class SpikyKernel
    {
    protected:
        static f32 m_radius;
        static f32 m_k;
        static f32 m_l;
        static f32 m_W_zero;
    public:
        static float getRadius() { return m_radius; }
        static void setRadius( float val )
        {
            m_radius = val;
            const float radius6 = pow( m_radius, 6 );
            static const float pi = static_cast<float>( M_PI );
            m_k = 15.0f / ( pi*radius6 );
            m_l = -45.0f / ( pi*radius6 );
            m_W_zero = W( Vector3F(0.f) );
        }

    public:

        /**
        * W(r,h) = 15/(pi*h^6) * (h-r)^3
        */
        static float W( const float r )
        {
            float res = 0.0;
            const float r2 = r*r;
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                const float hr3 = pow( m_radius - sqrt( r2 ), 3 );
                res = m_k * hr3;
            }
            return res;
        }

        static float W( const Vector3F &r )
        {
            float res = 0.0f;
            const float r2 = lengthSqr( r ); // .getAsFloat();
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                const float hr3 = pow( m_radius - sqrt( r2 ), 3 );
                res = m_k * hr3;
            }
            return res;
        }


        /**
        * grad(W(r,h)) = -r(45/(pi*h^6) * (h-r)^2)
        */
        static Vector3F gradW( const Vector3F &r )
        {
            Vector3F res(0.f);
            const float r2 = lengthSqr( r ); // .getAsFloat();
            const float radius2 = m_radius*m_radius;
            if( r2 <= radius2 )
            {
                const float r_l = sqrt( r2 );
                const float hr = m_radius - r_l;
                const float hr2 = hr*hr;
                const float normalizator = 1.f / r_l ;// ( r_l > FLT_EPSILON ) ? 1.f / r_l : 0.f;
                res = m_l * hr2 * r * normalizator;
            }
            //else
            //    res.setZero();

            return res;
        }

        static float W_zero()
        {
            return m_W_zero;
        }
    };
}
