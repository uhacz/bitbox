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
    float3 _cameraX;
    float3 _cameraY;
    float3 _cameraZ;
    float3 _cameraEye;
    float3 _sunDir;
    float3 _sunColor;
    float2 _resolution;
    float _aspect;
    float _time;
    
    uint _numSpheres;
};
Buffer<float4> _spheres: register(t0);
Buffer<float4> _colors : register(t1);
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
#define FLT_MAX 3.402823466e+38F
#define PI      3.14159265358979323846f
#define PI2     6.28318530717958647693f
#define PI_INV  0.31830988618379067154f

float hash( const float n ) 
{
    return frac( sin( n )*43758.54554213 );
}
float2 hash2( const float n )
{
    return frac( sin( float2( n, n + 1. ) )*float( 43758.5453123 ) );
}
float2 hash2( const float2 n )
{
    return frac( sin( float2( n.x*n.y, n.x + n.y ) )*float2( 25.1459123, 312.3490423 ) );
}
float3 hash3( const float2 n )
{
    return frac( sin( float3( n.x, n.y, n.x + 2.0 ) ) * float3( 36.5453123, 43.1459123, 11234.3490423 ) );
}

float2 rand2n( inout float2 seed ) 
{
    seed+=float2(-1,1);
	// implementation based on: lumina.sourceforge.net/Tutorials/Noise.html
    return float2(frac(sin(dot(seed.xy ,float2(12.9898,78.233))) * 43758.5453), frac(cos(dot(seed.xy ,float2(4.898,7.23))) * 23421.631) );
};



float interesctSphere( in float3 rO, in float3 rD, in float4 sph )
{
    const float3 p = rO - sph.xyz;
    const float b = dot( p, rD );
    const float c = dot( p, p ) - sph.w*sph.w;
    const float h = b*b - c;
    const float h1 = -b - sqrt( h );
    return ( h > 0 ) ? h1 : h;
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
float3 cosWeightedRandomHemisphereDirection2( in float3 n, inout float2 seed )
{
//    float2 xi = rand2n( seed );
////    float Xi2 = frand( seed );
//    
//    float  theta = acos( sqrt( 1.0f-xi.x ) );
//    float  phi = PI2 * xi.y;
//
//    float xs = sin(theta) * cos(phi);
//    float ys = cos(theta);
//    float zs = sin(theta) * sin(phi);
//
//    float3 y = n;
//    float3 h = ortho( y );
//
//    const float3 x = normalize( cross( h, y ) );
//    const float3 z = normalize( cross( x, y ) );
//    const float3 direction = xs * x + ys * y + zs * z;
//    return normalize( direction );

    float3  uu = normalize( cross( n, float3(0.0,1.0,1.0) ) );
	float3  vv = cross( uu, n );
	float2 rv2 = rand2n( seed );
	float ra = sqrt(rv2.y);
	float rx = ra*cos(PI2*rv2.x); 
	float ry = ra*sin(PI2*rv2.x);
	float rz = sqrt( 1.0-rv2.y );
	float3  rr = float3( rx*uu + ry*vv + rz*n );

    return normalize( rr );
}
float3 getCosineWeightedSample( in float3 dir, inout float2 seed )
{
    return cosWeightedRandomHemisphereDirection2( dir, seed );
}
float worldShadow( in float3 ro, in float3 rd, in float maxLen )
{
    const float2 tres = worldIntersect( ro, rd, maxLen );
    return (tres.y < 0.f) ? 1.f : 0.f;
}

float3 worldGetNormal( in float3 po, in float objectID )
{
    const float4 sph = _spheres[(int)objectID];
    return normalize( po - sph.xyz );

}
float3 worldGetColor( in float3 po, in float3 no, in float objectID )
{
    return _colors[(int)objectID].xyz;
}
float3 worldGetBackgound( in float3 rd ) 
{ 
    float sunAmount = saturate( dot( rd, -_sunDir ) );
    const float3 skyColor = float3( 0.4f, 0.5f, 1.0f );
    const float3 sunColor = _sunColor;
    return lerp( skyColor, sunColor, pow( sunAmount, 64.0 ) );
}

float3 worldApplyLighting( in float3 pos, in float3 nor )
{
    const float3 L = normalize( -_sunDir *1000.f - pos );
    const float sh = worldShadow( pos, L, 2000.f );
    return _sunColor * saturate( dot( nor, -_sunDir ) ) * sh;
}
float3 worldGetBRDFRay( in float3 pos, in float3 nor, in float3 eye, in float materialID, inout float2 seed )
{
    return getCosineWeightedSample( nor, seed );
}

float3 renderCalculateColor( in float3 rayOrig, in float3 rayDir, int nLevels, inout float2 seed )
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
        const float3 litCol = worldApplyLighting( wPos, wNrm ) * PI_INV;

        ro = wPos;
        //rd = normalize( reflect( wNrm, -rd ) );//worldGetBRDFRay( wPos, wNrm, rd, tres.y, rnd );    
        rd = worldGetBRDFRay( wPos, wNrm, rd, tres.y, seed );

        fcol *= matCol;
        tcol += fcol * litCol;
    }

    return tcol;
}

float3 calculatePixelColor( in float2 pixel, in float2 resolution, int nSamples, int nLevels, inout float2 seed )
{
    float3 color = (float3)( 0.f );

    for ( int isample = 0; isample < nSamples; ++isample )
    {
        const float2 shift = 0.5f * rand2n( seed ) / resolution.y + 0.5f;
        const float2 p = pixel + shift; //(-_resolution + (pixel ) * 2.0 ) / _resolution.y;

        const float3 rayDir = normalize( _cameraX * p.x + _cameraY * p.y - _cameraZ * 2.5f );
        const float3 rayOrg = _cameraEye;

        color += renderCalculateColor( rayOrg, rayDir, nLevels, seed );
    }

    color *= 1.f / float( nSamples );

    return color;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

float hash(float2 uv)
{
    float r;
    uv = abs(fmod(10.*frac((uv+1.1312)*31.),uv+2.));
    uv = abs(fmod(uv.x*frac((uv+1.721711)*17.),uv));
    return r = frac(10.* (7.*uv.y + 31.*uv.x));
}
#define MOD3 float3(.27232,.17369,.20787)
float2 hash22(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * MOD3);
    p3 += dot(p3.zxy, p3.yxz+19.19);
    return frac(float2(p3.x * p3.y, p3.z*p3.x));
}
float4 ps_pathtracer( in out_VS_screenquad input ) : SV_Target
{
    float2 seed = input.uv * _resolution;
    float2 pixel = input.screenPos;
    pixel.x *=  _aspect;

    seed = hash22( seed );
    seed = hash22( hash22( seed ) );

    //uint seed = asuint( hash2( pixel * _resolution + _time ).x );
    float3 col = calculatePixelColor( pixel, _resolution, 16, 2, seed );
    return float4(col, 1.0);
}