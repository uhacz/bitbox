#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/memory.h>
#include <util/common.h>
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
    Vector3 worldGetBRDFRay( const Vector3& pos, const Vector3& nor, const Vector3& eye, float materialID );
    Vector3 renderCalculateColor( const Vector3& ro, const Vector3& rd )
    {
        const Vector2 tres = worldIntersect( ro, rd, floatInVec( 1000.f ) );
        if ( tres.y < 0.0f )
            return worldGetBackgound( rd );

        const Vector3 wPos = ro + rd * tres.x;
        const Vector3 wNrm = worldGetNormal( wPos, tres.y );
        const Vector3 matCol = worldGetColor( wPos, wNrm, tres.y );
        const Vector3 litCol = worldApplyLighting( wPos, wNrm );

        return mulPerElem( matCol, litCol );
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
        const Vector3 eye = Vector3( 0.f, 0.f, 5.f );
        const Matrix3 cameraRot = Matrix3::identity();
        
        const Vector3 resolution = Vector3( (float)w, (float)h, 0.f );

        Vec3* c = (Vec3*)BX_MALLOC( bxDefaultAllocator(), w*h *sizeof( Vec3 ), 4 );
        int i = 0;
        for ( unsigned short y = 0; y<h; ++y )
        {            // Loop over image rows
            for ( unsigned short x = 0; x<w; x++ )   // Loop cols
            {
                const Vector3 pixel = Vector3( (float)x / (float)w, 1.f - ( (float)y / (float)h ), 0.f ) * twoVec - Vector3(1.f);
                const Vector3 p = pixel * resolution.getX() / resolution.getY();

                const Vector3 rayDir = normalize( cameraRot.getCol0() * p.getX() + cameraRot.getCol1() * p.getY() - cameraRot.getCol2() * 2.5f );
                const Vector3 col = renderCalculateColor( eye, rayDir );
                
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
    bxPathTracer::doPathTracing( 512, 512, 4, 1 );
    return 0;
}