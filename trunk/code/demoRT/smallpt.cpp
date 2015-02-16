#include <math.h>   // smallpt, a Path Tracer by Kevin Beason, 2008
#include <stdlib.h> // Make : g++ -O3 -fopenmp smallpt.cpp -o smallpt
#include <stdio.h>  //        Remove "-fopenmp" for g++ version < 4.2
#include <stdint.h>

#define FLOAT_PI 3.14159265358979323846f
#define EPSILON 0.7f
#define zerov(v) (((v).x = 0.f) ,((v).y = 0.f) , ((v).z = 0.f))
#define allonev(v) (((v).x = 1.f) , ((v).y = 1.f) , ((v).z = 1.f))
#define vdot(a, b) ((a).x * (b).x + (a).y * (b).y + (a).z * (b).z)
#define fsign(a) ((a)>=0.0f?1.0f:-1.0f)
#define max(a,b) (((a)>=(b))?(a):(b))

static uint32_t mirand = 1;

float frand()
{
    unsigned int a;

    mirand *= 16807;

    a = (mirand & 0x007fffff) | 0x40000000;

    return(*((float*)&a) - 2.0f)*0.5f;
}


inline float frand2()
{
    unsigned int a;

    mirand *= 16807;

    a = (mirand & 0x007fffff) | 0x40000000;

    return(*((float*)&a) - 2.0f);
}

inline float frand_11()
{
    unsigned int a;

    mirand *= 16807;

    a = (mirand & 0x007fffff) | 0x40000000;

    return(*((float*)&a) - 3.0f);
}

inline float approx_pow( const float x, const float y )
{
    union
    {
        float f;
        int i;
    } _eco;

    _eco.f = x;
    _eco.i = (int)(y*(_eco.i - 1065353216) + 1065353216);
    return _eco.f;
}

inline float approx_sqrt( const float x )
{
    union
    {
        float f;
        int i;
    } _eco;

    _eco.f = x;
    _eco.i = ((_eco.i - 1065353216) >> 1) + 1065353216;
    return _eco.f;
}

inline float fast_invSqrt( const float v )
{
    float y = v;
    int i = *(int *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;

    y *= (1.5f - (0.5f * v * y * y));
    float w = v*y*y;
    y *= (1.875f - (1.25f - 0.375f * w) * w);
    return y;
}

inline float fast_invSqrt2( const float v )
{
    float y = v;
    int i = *(int *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;

    y *= (1.5f - (0.5f * v * y * y));
    return y;
}

inline float fast_sqrt( const float x )
{
    return x * fast_invSqrt2( x );
}

struct Vec {        // Usage: time ./smallpt 5000 && xv image.ppm
    float x, y, z;                  // position, also color (r,g,b)
    inline Vec( float x_ = 0, float y_ = 0, float z_ = 0 ){ x = x_; y = y_; z = z_; }
    inline Vec& operator=(const Vec &b) { x = b.x, y = b.y, z = b.z; return *this; }
    inline Vec operator+(const Vec &b) const { return Vec( x + b.x, y + b.y, z + b.z ); }
    inline Vec operator+=(const Vec &b) { x += b.x, y += b.y, z += b.z; return *this; }
    inline Vec operator-(const Vec &b) const { return Vec( x - b.x, y - b.y, z - b.z ); }
    inline Vec operator-=(const Vec &b) { x -= b.x, y -= b.y, z -= b.z; return *this; }
    inline Vec operator*=(const Vec &b) { x *= b.x, y *= b.y, z *= b.z; return *this; }
    inline Vec operator*(float b) const { return Vec( x*b, y*b, z*b ); }
    inline Vec operator*=(float b) { x *= b, y *= b, z *= b; return *this; }
    inline Vec inverse() const { return Vec( -x, -y, -z ); }
    inline Vec mult( const Vec &b ) const { return Vec( x*b.x, y*b.y, z*b.z ); }
    inline Vec& norm(){ return *this = *this * (fast_invSqrt2( x*x + y*y + z*z )); }
    inline Vec normCopy(){ return *this * (fast_invSqrt2( x*x + y*y + z*z )); }
    inline float dot( const Vec &b ) const { return x*b.x + y*b.y + z*b.z; } // cross:
    inline Vec operator%(Vec&b){ return Vec( y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x ); }
};
struct Ray { Vec o, d; Ray( Vec o_, Vec d_ ) : o( o_ ), d( d_ ) {} };
enum Refl_t { DIFF, SPEC, REFR };  // material types, used in radiance()
struct Sphere {
    float rad;       // radius
    Vec p, e, c;      // position, emission, color
    Refl_t refl;      // reflection type (DIFFuse, SPECular, REFRactive)

    Sphere( float rad_, Vec p_, Vec e_, Vec c_, Refl_t refl_ ) :
        rad( rad_ ), p( p_ ), e( e_ ), c( c_ ), refl( refl_ ) {}

    float intersect( const Ray &r ) const { // returns distance, 0 if nohit
        Vec op;
        op = p;
        op -= r.o; // Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0
        float b = op.dot( r.d ), det = b*b - vdot( op, op ) + rad*rad;
        if ( det < 0.f )
            return 0.f;
        else
            det = sqrtf( det );

        float t = b - det;
        if ( t >  EPSILON )
            return t;
        else {
            t = b + det;

            if ( t >  EPSILON )
                return t;
            else
                return 0.f;
        }
    }

};

Sphere spheres[] = {//Scene: radius, position, emission, color, material
    Sphere( 1e5f, Vec( 1e5f + 1, 40.8f, 81.6f ), Vec(), Vec( .75f, .25f, .25f ), DIFF ),//Left
    Sphere( 1e5f, Vec( -1e5f + 99, 40.8f, 81.6f ), Vec(), Vec( .25f, .25f, .75f ), DIFF ),//Rght
    Sphere( 1e5f, Vec( 50.f, 40.8f, 1e5f ), Vec(), Vec( .75f, .75f, .75f ), DIFF ),//Back
    Sphere( 1e5f, Vec( 50.f, 40.8f, -1e5f + 170 ), Vec(), Vec(), DIFF ),//Frnt
    Sphere( 1e5f, Vec( 50.f, 1e5f, 81.6f ), Vec(), Vec( .75f, .75f, .75f ), DIFF ),//Botm
    Sphere( 1e5f, Vec( 50.f, -1e5f + 81.6f, 81.6f ), Vec(), Vec( .75f, .75f, .75f ), DIFF ),//Top
    Sphere( 16.5f, Vec( 27.f, 16.5f, 47.f ), Vec(), Vec( 1, 1, 1 )*.999f, DIFF ),//Mirr
    Sphere( 16.5f, Vec( 73.f, 16.5f, 78.f ), Vec(), Vec( 1, 1, 1 )*.999f, DIFF ),//Glas
    Sphere( 7, Vec( 50.f, 66.6f, 81.6f ), Vec( 12, 12, 12 ), Vec(), DIFF ) //Lite
};

static const int sphereCount = (int) sizeof( spheres ) / sizeof( Sphere );
inline float clamp( float x ){ return x<0.0f ? 0.0f : x>1.0f ? 1.0f : x; }
inline int toInt( float x ){ return int( approx_pow( clamp( x ), 0.454545455f )*255.0f + .5f ); }
inline bool intersect( const Ray &r, float &t, int &id ){
    float d, inf = t = 1e20f;
    for ( unsigned short i = 0; i<sphereCount; i++ ) if ( (d = spheres[i].intersect( r )) && d<t ){ t = d; id = i; }
    return t<inf;
}

inline bool IntersectP( const Ray &r, const float maxt ) {
    float d;
    for ( unsigned short i = 0; i<sphereCount; i++ ) {
        d = spheres[i].intersect( r );
        if ( (d != 0.f) && (d < maxt) )
            return true;
    }

    return false;
}

inline Vec UniformSampleSphere() {
    const float zz = frand_11();
    const float r = approx_sqrt( 1.f - zz * zz );
    const float cosphi = frand_11();
    const float xx = r * cosphi;
    const float yy = r * fsign( frand_11() )*approx_sqrt( 1.0f - cosphi*cosphi );

    return Vec( xx, yy, zz );
}


inline Vec SampleLights( const Vec &hitPoint, const Vec &normal ) {
    Vec result;
    /* For each light */
    for ( unsigned short i = 0; i<sphereCount; i++ ) {
        Sphere &light = spheres[i];
        if ( !light.e.x == 0.0f ) {
            /* It is a light source */

            /* Choose a point over the light source */
            Vec unitSpherePoint = UniformSampleSphere();
            Vec spherePoint = unitSpherePoint;
            spherePoint *= light.rad;
            spherePoint += light.p;

            /* Build the shadow ray direction */
            Ray shadowRay( hitPoint, spherePoint );
            shadowRay.d -= hitPoint;
            const float lensq = vdot( shadowRay.d, shadowRay.d );
            const float invlen = fast_invSqrt2( lensq );
            shadowRay.d *= invlen;

            float wo = vdot( shadowRay.d, unitSpherePoint );
            if ( wo > 0.f ) {
                // It is on the other half of the sphere 
                continue;
            }
            else
                wo = -wo;

            /* Check if the light is visible */
            const float wi = vdot( shadowRay.d, normal );
            if ( (wi > 0.f) && (!IntersectP( shadowRay, lensq*invlen - EPSILON )) ) {
                const float s = (4.f* FLOAT_PI* light.rad * light.rad) * wi * wo * (invlen*invlen);
                Vec c = light.e;
                c *= s;

                result += c;
            }
        }
    }
    return result;
}

static const float nc = 1.0f, nt = 1.5f, ncnt = nc / nt, ntnc = nt / nc;
static const float a = nt - nc, b = nt + nc;
static const float R0 = (a*a) / (b*b);
inline Vec radiance( const Ray &r ) {
    Ray currentRay = r;
    Vec rad = Vec();
    Vec throughput;
    allonev( throughput );

    unsigned int depth = 0;
    bool specularBounce = true;

    float t;                               // distance to intersection
    int id = 0;                               // id of intersected object
    int oldit = 0;
    Vec nl;
    Vec hitPoint;
    Sphere *obj = NULL;

    for ( ;; ++depth )
    {
        if ( depth>1 || !intersect( currentRay, t, id ) ) {
            /* Direct lighting component */
            if ( obj != NULL && obj->refl == DIFF ) {
                Vec Ld = SampleLights( hitPoint, nl );
                Ld *= throughput;
                rad += Ld;
            }
            return rad; // if miss, return
        }
        obj = &spheres[id];  // the hit object

        hitPoint = currentRay.d;
        hitPoint *= t;
        hitPoint += currentRay.o;

        Vec normal = hitPoint;
        normal -= obj->p;
        normal.norm();
        float dp = normal.dot( currentRay.d );

        /* Add emitted light */
        if ( obj->e.x != 0.0f ) {
            Vec eCol = obj->e;
            eCol *= fabs( dp );
            eCol *= throughput;
            rad += eCol;

            return rad;
        }

        nl = dp<0.0f ? normal : normal.inverse();

        if ( obj->refl == DIFF ) {                  // Ideal DIFFUSE reflection
            //specularBounce = false;
            throughput *= obj->c;

            float r1 = frand_11(), r2 = frand(), r2s = approx_sqrt( r2 ), r1r2s = r1*r2s;
            Vec w = nl;
            Vec u, v;
            if ( fabs( w.x )>.1f ) {
                v = w;
                v *= -w.y;
                v.y = 1.0f + v.y;
                u.x = w.z*r1r2s, u.y = 0.0f, u.z = -w.x*r1r2s;
            }
            else {
                v = w;
                v *= -w.x;
                v.x = 1.0f - v.x;
                u.x = 0.0f, u.y = -w.z*r1r2s, u.z = w.y*r1r2s;
            }

            Vec d = (u + v*(fsign( frand_11() )*approx_sqrt( 1.0f - r1*r1 )*r2s) + w*approx_sqrt( 1.0f - r2 )).norm();
            currentRay.o = hitPoint;
            currentRay.d = d;
            continue;
        }
        else if ( obj->refl == SPEC )            // Ideal SPECULAR reflection
        {
            specularBounce = true;
            throughput *= obj->c;
            currentRay.o = hitPoint;
            currentRay.d -= normal*(dp + dp);
            continue;
        }

        specularBounce = true;
        // Ideal dielectric REFRACTION
        bool into = (normal.x == nl.x);                // Ray from outside going in?
        float nnt = into ? ncnt : ntnc, ddn = currentRay.d.dot( nl );
        float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);
        if ( cos2t < 0.f )    // Total internal reflection
        {
            throughput *= obj->c;
            currentRay.o = hitPoint;
            currentRay.d -= normal*(dp + dp);
            continue;
        }

        Vec tdir = (currentRay.d*nnt - normal*((into ? 1.0f : -1.0f)*(ddn*nnt + approx_sqrt( cos2t )))).norm();

        float c = 1.0f - (into ? -ddn : tdir.dot( normal ));
        float cc = c*c;
        float Re = R0 + (1.0f - R0)*cc*cc*c;
        float P = .25f + .5f*Re;
        if ( frand() < P ) { /* R.R. */
            throughput *= (Re / P);
            throughput *= obj->c;

            currentRay.o = hitPoint;
            currentRay.d -= normal*(dp + dp);
            continue;
        }
        else {
            throughput *= (1.0f - Re) / (1.f - P);
            throughput *= obj->c;
            currentRay = Ray( hitPoint, tdir );
            continue;
        }
    }
}

int doPathTracing( int w, int h, int samps ){
    //int w = 640;
    //int h = 480;
    //int samps = 4; //argc==2 ? atoi(argv[1]) : 1; // # samples
    static const float a = 140.0f;
    static const float invw = 1.0f / w;
    static const float hinvw = 0.5f / w;
    static const float hinvh = 0.5f / h;
    static const float invsamps = 1.0f / ((float)samps);

    Ray cam( Vec( 50, 52, 295.6f ), Vec( 0, -0.042612f, -1 ).norm() ); // cam pos, dir
    Vec acamd = cam.d*a;
    Vec cx = Vec( w*.5135f / h )*a, cy = (cx%cam.d).norm()*.5135f*a, *c = new Vec[w*h];
    //#pragma omp parallel for schedule(dynamic, 2)      // OpenMP

    for ( unsigned short y = 0; y<h; ++y ){            // Loop over image rows
        Vec r;
        int i = (h - y - 1)*w;
        float sxp, syp;
        Vec d, dn;
        Ray cr( d, dn );
        syp = ((y + y) + 0.5f)*hinvh - 0.5f;
        sxp = (0.5f)*invw - 0.5f;
        fprintf( stderr, "\rRendering (%d samples/pp) %5.2f%%", samps, 100.*y / (h - 1) );
        for ( unsigned short x = 0; x<w; x++ )   // Loop cols
        {
            i++;
            sxp += invw;
            zerov( r );
            for ( unsigned short s = 0; s<samps; s++ ){
                d = acamd;
                d += cx*(sxp + frand_11()*hinvw);
                d += cy*(syp + frand_11()*hinvh);
                dn = d;
                dn.norm();
                d += cam.o;
                cr.o = d;
                cr.d = dn;
                r += radiance( cr );
            } // Camera rays are pushed ^^^^^ forward to start in interior
            r *= invsamps;
            c[i] = r;
        }
    }
    FILE *f = NULL;
    fopen_s( &f, "image.ppm", "w" );         // Write image to PPM file.
    fprintf( f, "P3\n%d %d\n%d\n", w, h, 255 );
    for ( int i = 0; i<w*h; i++ )
        fprintf( f, "%d %d %d ", toInt( c[i].x ), toInt( c[i].y ), toInt( c[i].z ) );

    fclose( f );

    return 0;
}
