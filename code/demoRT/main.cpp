#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/memory.h>
#include <util/common.h>
#include <util/random.h>
#include <util/time.h>
#include <stdio.h>

static const Vector4 spheres[] =
{
    Vector4( 0.f, 0.f, 0.f, 0.4f ),
    Vector4( 0.f,-1.f, 0.f, 0.5f ),
    Vector4( 0.f, -100.f, 0.f, 99.f ),
};
static const int nSpheres = sizeof( spheres ) / sizeof( *spheres );

static const Vector3 _sunDir = normalize( Vector3( 1.f, -1.f, 0.f ) );
static const Vector3 _sunColor = Vector3( 1.0f, 0.9f, 0.7f );

const Vector3 eye = Vector3( 0.f, 0.f, 5.f );
const Matrix3 cameraRot = Matrix3::identity();

namespace bxPathTracer
{
    floatInVec interesctSphere( const Vector3& rO, const Vector3& rD, const Vector4& sph )
    {
        const Vector3 p = rO - sph.getXYZ();
        const floatInVec b = dot( p, rD );
        const floatInVec c = dot( p, p ) - sph.getW()*sph.getW();
        const floatInVec h = b*b - c;
        const floatInVec h1 = -b - floatInVec( sqrtf4( h.get128() ) );
        
        const floatInVec result = select( h, h1, h > zeroVec );
        return result;
    }
    
    Vector2 worldIntersect( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen )
    {
        Vector2 result( FLT_MAX, -1.f );
        for( int isphere = 0; isphere < nSpheres; ++isphere )
        {
            const float t = interesctSphere( ro, rd, spheres[isphere] ).getAsFloat();
            if( ( t> 0.f && t < result.x ) )
            {
                result = Vector2( t, (float)isphere );
            }
        }
        return result;
    }

    Vector3 ortho( const Vector3& v ) 
    {
        //  See : http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts
        return select( Vector3( zeroVec, -v.getZ(), v.getY() ), Vector3(-v.getY(), v.getX(), zeroVec ), absf4( v.getX() ) > absf4( v.getZ() ) );
        //return abs(v.x) > abs(v.z) ? vec3(-v.y, v.x, 0.0)  : vec3(0.0, -v.z, v.y);
    }

    Vector3 getSampleBiased( const Vector3& dir, float power, bxRandomGen& rnd  ) 
    {
        const Vector3 dirN = normalize(dir);
        const Vector3 o1 = normalize(ortho(dirN));
        const Vector3 o2 = normalize(cross(dirN, o1));
        Vector2 r = Vector2( rnd.getf( 0.f, 1.f ), rnd.getf( 0.f,1.f) );
        r.x = r.x*2.f*PI;
        r.y = powf(r.y,1.0f/(power+1.0f));
        float oneminus = sqrt(1.0f-r.y*r.y);
        return o1 * cos(r.x)*oneminus + sin(r.x)*oneminus*o2 + r.y*dirN;
    }

    vector3f CosWeightedRandomHemisphereDirection2( normal3f n )
    {
        float Xi1 = (float)rand()/(float)RAND_MAX;
        float Xi2 = (float)rand()/(float)RAND_MAX;

        float  theta = acos(sqrt(1.0-Xi1));
        float  phi = 2.0 * 3.1415926535897932384626433832795 * Xi2;

        float xs = sinf(theta) * cosf(phi);
        float ys = cosf(theta);
        float zs = sinf(theta) * sinf(phi);

        vector3f y(n.x, n.y, n.z);
        vector3f h = y;
        if (fabs(h.x)<=fabs(h.y) && fabs(h.x)<=fabs(h.z))
            h.x= 1.0;
        else if (fabs(h.y)<=fabs(h.x) && fabs(h.y)<=fabs(h.z))
            h.y= 1.0;
        else
            h.z= 1.0;


        vector3f x = (h ^ y).normalize();
        vector3f z = (x ^ y).normalize();

        vector3f direction = xs * x + ys * y + zs * z;
        return direction.normalize();
    }

    Vector3 cosineSampleHemisphere(float u1, float u2)
    {
        const float r = sqrt(u1);
        const float theta = PI2 * u2;

        const float x = r * cos(theta);
        const float y = r * sin(theta);

        return normalize( Vector3(x, y, sqrt(maxOfPair(0.0f, 1.f - u1))) );
    }

    Vector3 getCosineWeightedSample( const Vector3& dir, bxRandomGen& rnd ) 
    {
        return cosineSampleHemisphere( rnd.getf(0.f,1.f), rnd.getf(0.f,1.f) );
        //return getSampleBiased( dir, 1.0f, rnd );
    }
    floatInVec worldShadow( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen );

    Vector3 worldGetNormal( const Vector3& po, float objectID )
    {
        const Vector4& sph = spheres[(int)objectID];
        return normalize( po - sph.getXYZ() );

    }
    Vector3 worldGetColor( const Vector3& po, const Vector3& no, float objectID )
    {
        return Vector3( 1.0f, 0.8f, 0.5f );

    }
    Vector3 worldGetBackgound( const Vector3& rd ) { (void)rd; return Vector3( 0.5f, 0.6f, 0.7f ); }

    Vector3 worldApplyLighting( const Vector3& pos, const Vector3& nor )
    {
        return _sunColor * clampf4( dot( nor, -_sunDir ), zeroVec, oneVec );
    }
    Vector3 worldGetBRDFRay( const Vector3& pos, const Vector3& nor, const Vector3& eye, float materialID, bxRandomGen& rnd )
    {
        return getCosineWeightedSample( nor, rnd );
    }
    Vector3 reflect( const Vector3& N, const Vector3& L )
    {
        return twoVec * dot( N, L ) * N - L;
    }
    Vector3 renderCalculateColor( const Vector3& rayOrig, const Vector3& rayDir, int nLevels, bxRandomGen& rnd )
    {
        Vector3 tcol(0.f);
        Vector3 fcol(1.f);

        Vector3 ro = rayOrig;
        Vector3 rd = rayDir;

        for( int ilevel = 0; ilevel < nLevels; ++ilevel )
        {
            const Vector2 tres = worldIntersect( ro, rd, floatInVec( 1000.f ) );
            if ( tres.y < 0.0f )
            {
                if( ilevel == 0 ) return worldGetBackgound( rd );    
                else break;
            }

            const Vector3 wPos = ro + rd * tres.x;
            const Vector3 wNrm = worldGetNormal( wPos, tres.y );
            const Vector3 matCol = worldGetColor( wPos, wNrm, tres.y );
            const Vector3 litCol = worldApplyLighting( wPos, wNrm );

            ro = wPos;
            //rd = normalize( reflect( wNrm, -rd ) );//worldGetBRDFRay( wPos, wNrm, rd, tres.y, rnd );
            rd = worldGetBRDFRay( wPos, wNrm, rd, tres.y, rnd );

            fcol = mulPerElem( fcol, matCol );
            //if( ilevel > 0 )
            tcol += mulPerElem( fcol, litCol );
        }
        
        return tcol;
    }

    Vector3 calculatePixelColor( const Vector3& pixel, const Vector3& resolution, int nSamples, int nLevels )
    {
        Vector3 color(0.f);

        bxRandomGen rnd( (u32)bxTime::ms() );

        for( int isample = 0; isample < nSamples; ++isample )
        {
            const Vector3 shift = bxRand::randomVector2( rnd, Vector3(0.f), Vector3(1.f) );
            //const Vector3 p = ( ( pixel ) * resolution.getX() / resolution.getY() ) * twoVec - Vector3( oneVec );
            const Vector3 p = ( -resolution + ( pixel + shift ) * twoVec ) / resolution.getY();
            
            const Vector3 rayDir = normalize( cameraRot.getCol0() * p.getX() + cameraRot.getCol1() * p.getY() - cameraRot.getCol2() * 2.5f );
            const Vector3 rayOrg = eye;

            color += renderCalculateColor( rayOrg, rayDir, nLevels, rnd );
        }

        color /= float( nSamples );

        return color;
    }
    
    union Vec3
    {
        f32 xyz[3];
        f32 rgb[3];
        struct
        {
            f32 r, g, b;
        };
        struct
        {
            f32 x, y, z;
        };

        explicit Vec3( f32 v ): r(v), g(v), b(v) {}
        Vec3( f32 v0, f32 v1, f32 v2 ): r(v0), g(v1), b(v2) {}
    };
    

    inline int toInt( float x ){ return int( powf( clamp( x, 0.f, 1.f ), 0.454545455f )*255.0f + .5f ); }

   

    void doPathTracing( int w, int h, int samps, int bounces )
    {
        const Vector3 resolution = Vector3( (float)w, (float)h, 1.f );
        Vec3* c = (Vec3*)BX_MALLOC( bxDefaultAllocator(), w*h *sizeof( Vec3 ), 4 );
        int i = 0;
        for ( unsigned short y = 0; y<h; ++y )
        {            // Loop over image rows
            for ( unsigned short x = 0; x<w; x++ )   // Loop cols
            {
                fprintf( stderr, "\rRendering (%d samples/pp) %5.2f%%", samps, 100.*y / (h - 1) );
                const Vector3 pixel = Vector3( (float)x, (float)( w - y ), 0.f );
                const Vector3 col = calculatePixelColor( pixel, resolution, samps, bounces );
                //const Vector3 p = pixel * resolution.getX() / resolution.getY();

                //const Vector3 rayDir = normalize( cameraRot.getCol0() * p.getX() + cameraRot.getCol1() * p.getY() - cameraRot.getCol2() * 2.5f );
                //const Vector3 col = renderCalculateColor( eye, rayDir );
                
                //for ( unsigned short s = 0; s<samps; s++ )
                //{
                //    
                //}
                m128_to_xyz( c[i].xyz, col.get128() );
                ++i;
                //c[i++] = Vec3(1.f);
            }
        }
        FILE *f = NULL;
        fopen_s( &f, "image.ppm", "w" );         // Write image to PPM file.
        fprintf( f, "P3\n%d %d\n%d\n", w, h, 255 );
        for ( int i = 0; i<w*h; i++ )
            fprintf( f, "%d %d %d ", toInt( c[i].x ), toInt( c[i].y ), toInt( c[i].z ) );

        fclose( f );

        BX_FREE0( bxDefaultAllocator(), c );
    }
}///



int main()
{
    bxPathTracer::doPathTracing( 512, 512, 32, 5 );
    return 0;
}