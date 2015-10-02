passes:
{
    background =
    {
        vertex = "vs_screenquad";
        pixel = "ps_background";
    };

    foreground = 
    {
        vertex = "vs_foreground";
        pixel = "ps_foreground";
    };
}; #~header

#include <sys/vs_screenquad.hlsl>

#define in_PS out_VS_screenquad

shared cbuffer MaterialData: register( b3 )
{
    float2 inResolution;
    float2 inResolutionRcp;
    float  inTime;
};

Texture2D texNoise;
Texture2D texImage;
Texture2D texBackground;
Texture2D texLogo;
Texture2D texMaskHi;
Texture2D texMaskLo;
Texture1D texFFT;
SamplerState samplerNearest;
SamplerState samplerLinear;
SamplerState samplerBilinear;
SamplerState samplerBilinearBorder;

// rendering params
//static const float sphsize = 0.5; // planet size
static const float3 boxsize = float3( 1.8f, 1.8f / 1.9433, .1 );
static const float dist = .015; // distance for glow and distortion
static const float perturb = 0.9; // distortion amount of the flow around the planet
static const float displacement = .015; // hot air effect
static const float windspeed = .1; // speed of wind flow
static const float steps = 128; // number of steps for the volumetric rendering
static const float stepsize = .015;
static const float brightness = .43;
//static const float3 planetcolor = float3( 0.55, 0.4, 0.3 );
static const float fade = .007; //fade by distance
static const float glow = 3.5; // glow amount, mainly on hit side

// fractal params
static const int iterations = 13;
static const float fractparam = .7;
static const float3 offset = float3( 1.5, 2., -1.5 );

/*
#define WIN_WIDTH 0.5
#define WIN_HEIGHT 0.5


float linearstep( float a, float b, float t )
{
return ( clamp( t, a, b ) - a ) / (b-a);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
vec2 q = fragCoord.xy / iResolution.xy;
vec2 uv = q * 0.5;

vec2 wbegin = vec2( 0.25, 0.25 );
vec2 wend = wbegin + vec2( WIN_WIDTH, WIN_HEIGHT );

vec2 zbegin = vec2( 0.0, 0.0 );
vec2 zend = vec2( 1.0, 1.0 );

vec3 col = vec3(0.0,0.0,0.0);
if( q.x > wbegin.x && q.y > wbegin.y && q.x < wend.x && q.y < wend.y )
{
vec2 tx;
tx.x = linearstep( wbegin.x, wend.x * 2.0, q.x + 0.20 );
tx.y = linearstep( wbegin.y, wend.y * 2.0, q.y + 0.0 );

col = texture2D( iChannel0, vec2(tx.x,1.0-tx.y) ).xyz;
}

fragColor = vec4(col,1.0);
}
*/

float sdBox( float3 p, float3 b )
{
    float3 d = abs( p ) - b;
    return min( max( d.x, max( d.y, d.z ) ), 0.0 ) + length( max( d, 0.0 ) );
}

float wind( float3 p, int iter )
{
    p += float3( 0., inTime*windspeed, 0. ); // flow movement
    p = abs( frac( ( p + offset )*.1 ) - .15 ); // tile folding 
    for( int i = 0; i<iter; i++ )
    {
        p = abs( p ) / dot( p, p ) - fractparam; // the magic formula for the hot flow
    }
    return length( p ); // *(1. + d*glow*x) + d*glow*x; // return the result with glow applied
}

float linearstep( in float tmin, in float tmax, in float t )
{
    return (clamp( t, tmin, tmax ) - tmin) / (tmax - tmin);
}

float4 ps_background( in_PS IN ) : SV_Target0
{
    float2 uv = IN.uv - 0.5f;

    float fft = texFFT.Sample( samplerNearest, IN.uv ).r;
    float fft0  = texFFT.Sample( samplerNearest, 0.0f ).r;
    float fft05 = texFFT.Sample( samplerNearest, 0.05f ).r;
    float fft1  = texFFT.Sample( samplerNearest, 0.1f ).r;
    float fft2  = texFFT.Sample( samplerNearest, 0.2f ).r;
    float fft3  = texFFT.Sample( samplerNearest, 0.3f ).r;
    float fft4  = texFFT.Sample( samplerNearest, 0.4f ).r;
    float fft5  = texFFT.Sample( samplerNearest, 0.5f ).r;
    float fft6  = texFFT.Sample( samplerNearest, 0.6f ).r;
    float fft7  = texFFT.Sample( samplerNearest, 0.7f ).r;
    float fft8  = texFFT.Sample( samplerNearest, 0.8f ).r;
    float fft9  = texFFT.Sample( samplerNearest, 0.9f ).r;
    float fft10 = texFFT.Sample( samplerNearest, 1.0f ).r;

    float2 imgUV = IN.uv; // +( ( float2( fft3, fft2 ) - 0.5f ) * 2.0f ) * 0.025f;
    //imgUV.x = linearstep( 0.07, 0.93, IN.uv.x ) + fft3*0.05f;
    //imgUV.y = linearstep( 0.1, 0.9, IN.uv.y ) + fft2*0.05f;

    float4 maskHi = texMaskHi.Sample( samplerLinear, imgUV );
    float4 maskLo = texMaskLo.Sample( samplerLinear, float2( imgUV.x + sqrt( fft0 ) * sin( inTime * 10.f + imgUV.y * 55.f ) * 0.001f, imgUV.y ) );

    float3 dir = float3(uv, 1.);
    //dir.xy += ((fft2.rr * 0.5f - 0.5f) * fft2);

    dir.x *= inResolution.x / inResolution.y;
    float3 from = float3(0., 0., -2. + texNoise.Sample( samplerLinear, uv*.5 + inTime ).x*stepsize); //from+dither

        // volumetric rendering
    float t = inTime*windspeed*.2;
    float v = 0.0;
    float4 l = (float4)-0.0001f;
    //float v = 0., l = -0.0001, t = ;
    for( float r = 10.; r<steps; r++ )
    {
        float3 p = from + r*dir*stepsize;

        float tx = texNoise.Sample( samplerLinear, uv*.2 + float2( t, t ) ).x*displacement * fft5; // hot air effect
        //float box = length( max( abs( p ) - boxsize, 0.0 ) ) - tx;
        float box = sdBox( p, boxsize ) - tx;
        if( maskLo.x > 0.0 )
        //if( box > 0.01 )
        {
            // outside planet, accumulate values as ray goes, applying distance fading
            v += min( 10., wind( p, iterations ) ) * max( 0.0, 1.0 - r*fade );
        }
        //else// if( l.x < 0.0 )
        {
            //inside planet, get planet shading if not already 
            //loop continues because of previous problems with breaks and not always optimizes much
            
            float2 offR = float2(fft7, fft9);
            offR = ((offR - 0.5f) * 2.0f) * offR;

            float2 offB = float2(fft8, fft10);
            offB = ((offB - 0.5f) * 2.0f) * offB;

            float imgG = texImage.SampleLevel( samplerBilinearBorder, imgUV, 0.0 ).g;
            float imgR = texImage.SampleLevel( samplerBilinearBorder, imgUV + offR * 0.1f * sqrt( maskHi.r ), 0.0 ).r;
            float imgB = texImage.SampleLevel( samplerBilinearBorder, imgUV + offB * 0.1f * sqrt( maskHi.b ), 0.0 ).b;
            
            float4 logo = texLogo.SampleLevel( samplerLinear, imgUV - ( offR + offB ) * 0.01f, 0.0 );
            float4 img = lerp( float4(imgR, imgG, imgB, 1.0), logo, logo.a * saturate( fft05 ) );
            //img += texImage.SampleLevel( samplerBilinearBorder, imgUV + wind( p ) * 0.01f, 0.0 ) * ( 1 - maskLo );

            //img += maskLo * (fft0)* 50.f;
            l = img; // pow( max( .53, dot( normalize( p ), normalize( float3(0.1, -1.0, -0.3) ) ) ), 4. ) * (img * 2.5f);
            //v -= length( img ) * 50.;
            v += min( 40., wind( p, 13 ) ) * max( 0.0, 1.0 - r*fade ) * 500.0;
            v *= sqrt( maskLo ) * sqrt(fft0) * 2;
        }
    }
    v /= steps; v *= brightness; // average values and apply bright factor
    float3 col = float3(v*1.5, v*v, v*v*v) + l;// *planetcolor; // set color

    //col *= saturate( 1.0 - length( pow( abs( uv ), 5 * ( 1.f - fft1 ) ) )*16.0 ); // vignette (kind of)
    float4 color = float4( col, 1.0 );
    //float4 color = texImage.SampleLevel( samplerBilinear, IN.uv, 0.0 );

    return color; // +fft;
}


struct out_VS_foreground
{
    noperspective float2 screenPos	: TEXCOORD0;
    noperspective float2 uv         : TEXCOORD1;
};

out_VS_foreground vs_foreground(
    in float4 IN_pos : POSITION,
    in float2 IN_uv : TEXCOORD0,
    out float4 OUT_hpos : SV_Position )
{
    //float scale = 0.6f;
        
    float3 hpos = IN_pos.xyz;
    OUT_hpos = float4( hpos, 1.0 );

    out_VS_screenquad output;
    output.screenPos = hpos.xy;
    output.uv = IN_uv.xy;
    output.uv.y = 1.0 - output.uv.y;
    return output;
}

float4 ps_foreground( out_VS_foreground IN ) : SV_Target0
{
    float fft = texFFT.Sample( samplerNearest, IN.uv ).r;
    float fft0 = texFFT.Sample( samplerNearest, 0.0f ).r;
    float fft05 = texFFT.Sample( samplerNearest, 0.05f ).r;
    float fft1 = texFFT.Sample( samplerNearest, 0.1f ).r;
    float fft2 = texFFT.Sample( samplerNearest, 0.2f ).r;
    float fft3 = texFFT.Sample( samplerNearest, 0.3f ).r;
    float fft4 = texFFT.Sample( samplerNearest, 0.4f ).r;
    float fft5 = texFFT.Sample( samplerNearest, 0.5f ).r;
    float fft6 = texFFT.Sample( samplerNearest, 0.6f ).r;
    float fft7 = texFFT.Sample( samplerNearest, 0.7f ).r;
    float fft8 = texFFT.Sample( samplerNearest, 0.8f ).r;
    float fft9 = texFFT.Sample( samplerNearest, 0.9f ).r;
    float fft10 = texFFT.Sample( samplerNearest, 1.0f ).r;
    
    float zoom = 1.0f + ( sin( inTime ) * 0.5f + 0.5f ) * 0.5f;
    float2 target = float2( 0.1, 0.1 );

    float targetStrength = linearstep( 1.f, 1.5f, zoom );
    target = lerp( float2( 0, 0 ), target, targetStrength );

    float fft0s  = fft0 - 0.5 * 2.0;
    float fft5s  = fft5 - 0.5 * 2.0;
    float2 uv = ( IN.uv / zoom + target ) + ( float2( fft0s, fft5s ) * fft10 ) * 0.015;

    float4 color = texBackground.SampleLevel( samplerBilinear, uv, 0.0 );

    float2 q = uv - 0.5f;
    color *= saturate( 1.0 - length( pow( abs( q ), 5 * ( 1.f - fft1 ) ) )*16.0 ); // vignette (kind of)
    return color;
}