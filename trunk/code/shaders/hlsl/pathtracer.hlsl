passes:
{
    doPathTrace =
    {
        vertex = "vs_screenquad";
        pixel = "ps_pathtracer";
        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
    
}; #~header

#include <sys/vs_screenquad.hlsl>

shared cbuffer MaterialData : register(b3)
{
    float4x4 _camera_rot;
    float3 _camera_eye;
    float3 _sunDir;
    float3 _sunColor;
    float2 _resolution;
    float _time;
    
    uint _numSpheres;
};
Buffer<float4> _spheres: register(t0);
Buffer<float3> _colors : register(t1);
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
#define FLT_MAX 3.402823466e+38F
#define PI      3.14159265358979323846f
#define PI2     6.28318530717958647693f
#define PI_INV  0.31830988618379067154f

//#define N_SPHERES 9
//const float4 spheres[N_SPHERES] =
//{
//    float4( -4.f, 0.1f, 1.f, 0.5f ),
//    float4( -3.f, 0.2f, -2.f, 1.5f ),
//    float4( -2.f, -0.5f, 2.f, 1.1f ),
//    float4( -1.f, -0.3f, 1.f, 1.2f ),
//    float4( 0.f, 0.4f, -2.f, 2.3f ),
//    float4( 1.f, -0.5f, 1.f, 0.5f ),
//    float4( 2.f, 0.1f, .5f, 0.72f ),
//    float4( 3.f, -0.2f, 0.f, 0.1f ),
//    float4( 0.f, -51.f, 0.f, 50.f ),
//};
//
//static const float3 colors[N_SPHERES] =
//{
//    float3( 1.f, 0.f, 0.f ),
//    float3( 0.f, 1.f, 0.f ),
//    float3( 0.f, 0.f, 1.f ),
//    float3( 1.f, 1.f, 0.f ),
//    float3( 1.f, 0.f, 1.f ),
//    float3( 0.f, 1.f, 1.f ),
//    float3( 1.f, .5f, 0.f ),
//    float3( 0.f, .5f, .5f ),
//    float3( .8f, 0.f, 0.f ),
//};

//static const float3 _sunDir = normalize( float3(0.5f, -1.f, 0.f) );
//static const float3 _sunColor = float3(1.0f, 1.0f, 1.0f);
//
//const float3 _camera_eye = float3( 0.f, 0.f, 20.f );
//const float3x3 _camera_rot = {
//    1.f, 0.f, 0.f,
//    0.f, 1.f, 0.f,
//    0.f, 0.f, 1.f
//};
//
//const float2 _resolution = float2( 512, 512 );

float frand( inout int seed )
{
    seed *= 16807;
    uint ires = ((uint)seed >> 9 ) | 0x3f800000;
    return saturate( asfloat( ires ) - 1.0f );
}

float interesctSphere( in float3 rO, in float3 rD, in float4 sph )
{
    const float3 p = rO - sph.xyz;
    const float b = dot( p, rD );
    const float c = dot( p, p ) - sph.w*sph.w;
    const float h = b*b - c;
    const float h1 = -b - sqrt( h );
    return h > 0 ? h1 : h;
}

float2 worldIntersect( in float3 ro, in float3 rd, in float maxLen )
{
    float2 result = float2(  maxLen, -1.f );
    for ( uint isphere = 0; isphere < _numSpheres; ++isphere )
    {
        float t = interesctSphere( ro, rd, _spheres[isphere] );
        if ( (t> 0.f && t < result.x) )
        {
            result = float2( t, (float)isphere );
        }
    }
    return result;
}
float3 ortho( in float3 v )
{
    //  See : http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts
    return abs( v.x ) > abs( v.z ) ? float3(-v.y, v.x, 0.0) : float3(0.0, -v.z, v.y);
}
float3 cosWeightedRandomHemisphereDirection2( in float3 n, inout int seed )
{
    float Xi1 = frand( seed );
    float Xi2 = frand( seed );
    
    float  theta = acos( sqrt( 1.0f-Xi1 ) );
    float  phi = PI2 * Xi2;

    float xs = sin(theta) * cos(phi);
    float ys = cos(theta);
    float zs = sin(theta) * sin(phi);

    float3 y = n;
    float3 h = ortho( y );

    const float3 x = normalize( cross( h, y ) );
    const float3 z = normalize( cross( x, y ) );
    const float3 direction = xs * x + ys * y + zs * z;
    return normalize( direction );
}
float3 getCosineWeightedSample( in float3 dir, inout int seed )
{
    return cosWeightedRandomHemisphereDirection2( dir, seed );
}
float worldShadow( in float3 ro, in float3 rd, in float maxLen )
{
    const float2 tres = worldIntersect( ro, rd, maxLen );
    return (tres.y < 0.f) ? 0.f : 1.f;
}

float3 worldGetNormal( in float3 po, in float objectID )
{
    const float4 sph = _spheres[(int)objectID];
    return normalize( po - sph.xyz );

}
float3 worldGetColor( in float3 po, in float3 no, in float objectID )
{
    return _colors[(int)objectID];
}
float3 worldGetBackgound( in float3 rd ) 
{ 
    return float3(0.5f, 0.6f, 0.7f); 
}

float3 worldApplyLighting( in float3 pos, in float3 nor )
{
    const float3 L = normalize( -_sunDir *1000.f - pos );
    const float sh = worldShadow( pos, L, 2000.f );
    return _sunColor * saturate( dot( nor, -_sunDir ) ) * sh;
}
float3 worldGetBRDFRay( in float3 pos, in float3 nor, in float3 eye, in float materialID, inout int seed )
{
    return getCosineWeightedSample( nor, seed );
}

float3 renderCalculateColor( in float3 rayOrig, in float3 rayDir, int nLevels, inout int seed )
{
    float3 tcol = (float3)( 0.f );
    float3 fcol = (float3)( 1.f );

    float3 ro = rayOrig;
    float3 rd = rayDir;

    for ( int ilevel = 0; ilevel < nLevels; ++ilevel )
    {
        const float2 tres = worldIntersect( ro, rd, 1000.f );
        if ( tres.y < 0.0f )
        {
            if ( ilevel == 0 ) return worldGetBackgound( rd );
            else break;
        }

        const float3 wPos = ro + rd * tres.x;
        const float3 wNrm = worldGetNormal( wPos, tres.y );
        const float3 matCol = worldGetColor( wPos, wNrm, tres.y );
        const float3 litCol = worldApplyLighting( wPos, wNrm ) * matCol * PI_INV;

        ro = wPos;
        //rd = normalize( reflect( wNrm, -rd ) );//worldGetBRDFRay( wPos, wNrm, rd, tres.y, rnd );    
        rd = worldGetBRDFRay( wPos, wNrm, rd, tres.y, seed );

        fcol = fcol * matCol;
        tcol += fcol * litCol;
    }

    return tcol;
}

float3 calculatePixelColor( in float2 pixel, in float2 resolution, int nSamples, int nLevels, inout int seed )
{
    float3 color = (float3)( 0.f );

    for ( int isample = 0; isample < nSamples; ++isample )
    {
        const float2 shift = float2(frand( seed ), frand( seed ) );
        const float2 p = (-_resolution + (pixel + shift) * 2.0 ) / _resolution.y;

        const float3 rayDir = normalize( _camera_rot[0].xyz * p.x + _camera_rot[1].xyz * p.y - _camera_rot[2].xyz * 2.5f );
        const float3 rayOrg = _camera_eye;

        color += renderCalculateColor( rayOrg, rayDir, nLevels, seed );
    }

    color *= 1.f / float( nSamples );

    return color;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
float hash( const float n ) 
{
    return fract( sin( n )*43758.54554213 );
}
float2 hash2( const float n )
{
    return fract( sin( vec2( n, n + 1. ) )*vec2( 43758.5453123 ) );
}
float2 hash2( const float2 n )
{
    return fract( sin( vec2( n.x*n.y, n.x + n.y ) )*vec2( 25.1459123, 312.3490423 ) );
}
float3 hash3( const float2 n )
{
    return fract( sin( float3(n.x, n.y, n + 2.0) )*vec3( 36.5453123, 43.1459123, 11234.3490423 ) );
}
float4 ps_pathtracer( in out_VS_screenquad input ) : SV_Target
{
    uint seed = _time; // asuint( hash12( input.uv * _resolution + _time ) );
    float2 pixel = input.uv * _resolution;

    float3 col = calculatePixelColor( pixel, _resolution, 4, 5, seed );
    
    return float4(col, 1.0);
}