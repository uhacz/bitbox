#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <util\type.h>
#include <util\vectormath\vectormath.h>

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

		static Vector3 gradW(const Vector3 r)
		{
			Vector3 res;
			const f32 rl = length( r ).getAsFloat();
			const f32 q = rl / m_radius;
#ifndef NO_DISTANCE_TEST
            if( q <= 1.0 )
            #endif
            {
                if( rl > 1.0e-6 )
                {
                    const Vector3 gradq = r * ( ( f32 ) 1.0 / ( rl*m_radius ) );
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
                res = Vector3( 0.f );
#endif

			return res;
		}

		static f32 W_zero()
		{
			return m_W_zero;
		}
	};
}
