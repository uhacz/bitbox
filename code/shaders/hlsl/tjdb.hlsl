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

    text =
    {
        vertex = "vs_screenquad";
        pixel = "ps_text";

        hwstate =
        {
            blend_enable = 1;
            blend_src_factor = "ONE";
            blend_dst_factor = "ONE_MINUS_SRC_ALPHA";
            blend_equation = "ADD";
        };
    };
}; #~header

#include <sys/vs_screenquad.hlsl>

#define in_PS out_VS_screenquad

shared cbuffer MaterialData: register( b3 )
{
    float2 inResolution;
    float2 inResolutionRcp;
    float  inTime;
    float  fadeValueInv;
};

Texture2D texNoise;
Texture2D texImage;
Texture2D texBackground;
Texture2D texLogo;
Texture2D texText;
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

    float fft = texFFT.Sample( samplerNearest, IN.uv.x ).r;
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

    
    float t = inTime*windspeed*.2;
    float v = 0.0;
    float4 l = (float4) - 0.0001f;
    // volumetric rendering
    if ( maskLo.x > 0.0 )
    {
        [loop]
        for( float r = 10.; r<steps; r++ )
        {
            float3 p = from + r*dir*stepsize;
            float tx = texNoise.Sample( samplerLinear, uv*.2 + float2( t, t ) ).x*displacement * fft5; // hot air effect
            v += min( 10., wind( p, iterations ) ) * max( 0.0, 1.0 - r*fade );
            v += min( 40., wind( p, 13 ) ) * max( 0.0, 1.0 - r*fade ) * 500.0;
            v *= sqrt( maskLo.x ) * sqrt(fft0) * 2;
        }
    }
    {    
        float2 offR = float2(fft7, fft9);
        offR = ((offR - 0.5f) * 2.0f) * offR;

        float2 offB = float2(fft8, fft10);
        offB = ((offB - 0.5f) * 2.0f) * offB;

        float imgG = texImage.SampleLevel( samplerBilinearBorder, imgUV, 0.0 ).g;
        float imgR = texImage.SampleLevel( samplerBilinearBorder, imgUV + offR * 0.1f * sqrt( maskHi.r ), 0.0 ).r;
        float imgB = texImage.SampleLevel( samplerBilinearBorder, imgUV + offB * 0.1f * sqrt( maskHi.b ), 0.0 ).b;

        float4 logo = texLogo.SampleLevel( samplerLinear, imgUV - (offR + offB) * 0.01f, 0.0 );
        float4 img = lerp( float4(imgR, imgG, imgB, 1.0), logo, logo.a * saturate( fft05 ) );
        l = img;
    }   
    v /= steps; v *= brightness; // average values and apply bright factor
    float3 col = float3(v*1.5, v*v, v*v*v) + l.xyz;// *planetcolor; // set color

    //col *= saturate( 1.0 - length( pow( abs( uv ), 5 * ( 1.f - fft1 ) ) )*16.0 ); // vignette (kind of)
    float4 color = float4( col, 1.0 );
    //float4 color = texImage.SampleLevel( samplerBilinear, IN.uv, 0.0 );

    return color; // +fft;
}

float4 ps_text( in_PS IN ) : SV_Target0
{
    float4 txt = texText.SampleLevel( samplerBilinear, IN.uv, 0.0 );
    return txt;
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

////////////////
float rand( float2 co )
{
    return frac( sin( dot( co.xy, float2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

float rand( float c )
{
    return rand( float2( c, 1.0 ) );
}

float randomLine( float seed, float2 uv )
{
    float b = 0.01 * rand( seed );
    float a = rand( seed + 1.0 );
    float c = rand( seed + 2.0 ) - 0.5;
    float mu = rand( seed + 3.0 );

    float l = 1.0;

    if ( mu > 0.2 )
        l = pow( abs( a * uv.x + b * uv.y + c ), 1.0 / 8.0 );
    else
        l = 2.0 - pow( abs( a * uv.x + b * uv.y + c ), 1.0 / 8.0 );

    return lerp( 0.5, 1.0, l );
}
// Generate some blotches.
float randomBlotch( float seed, float2 uv )
{
    float x = rand( seed );
    float y = rand( seed + 1.0 );
    float s = 0.01 * rand( seed + 2.0 );

    float2 p = float2( x, y ) - uv;
    p.x *= inResolution.x / inResolution.y;
    float a = atan2( p.y, p.x );
    float v = 1.0;
    float ss = s*s * (sin( 6.2831*a*x )*0.1 + 1.0);

    if ( dot( p, p ) < ss )
        v = 0.2;
    else
        v = pow( dot( p, p ) - ss, 1.0 / 16.0 );

    return lerp( 0.3 + 0.2 * (1.0 - (s / 0.02)), 1.0, v );
}
////////////////

float4 ps_foreground( out_VS_foreground IN ) : SV_Target0
{
    float fft = texFFT.Sample( samplerNearest, IN.uv.x ).r;
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
    
    //float zoom = 1.0f + ( sin( cos( inTime * 0.12f) * sin( inTime * 0.051 ) ) * 0.5 + 0.5 ) * 0.75f; // +(sin( inTime ) * 0.5f + 0.5f) * 0.5f;
    //float2 target;
    //target.x = smoothstep( -1.f, 1.f, sin( -inTime * 0.125f ) ) * 0.3;
    //target.y = smoothstep( -1.f, 1.f, sin( sin( inTime * 0.125 ) - cos( inTime * 0.125 ) ) ) * 0.3;

    float zoom = 2.f - smoothstep( 0.f, 30.f, inTime ) * 0.3f;
    float2 target = float2(0.05f, 0.025f);

    
    float targetStrength = linearstep( 1.f, 1.5f, zoom );
    target = lerp( float2( 0, 0 ), target, targetStrength );

    float fft0s  = ( fft0 - 0.5 ) * 2.0;
    float fft5s  = ( fft5 - 0.5 ) * 2.0;
    float2 uv = (IN.uv / zoom + target) + (float2(fft0s, fft5s) * fft10) * 0.015;

    float4 color = texBackground.SampleLevel( samplerBilinear, uv, 0.0 );
    

    /// text
    if ( inTime >= 5.0 && inTime < 25.f )
    {
        float3 tr[] =
        {
            float3(5.0 , 10.0,-0.3),
            float3(10.0, 15.0,-0.1),
            float3(15.0, 20.0, 0.1),
            float3(20.0, 25.0, 0.3),
        };

        uint tri = 0;
        for ( uint i = 0; i < 4; i++ )
        {
            if ( inTime >= tr[i].x && inTime < tr[i].y )
            {
                tri = i;
                break;
            }
        }

        float alpha = linearstep( tr[tri].x, tr[tri].y, inTime );
        float alphaPulseYRange = 1.f - alpha * alpha * alpha * alpha * alpha;
        
        float2 yRange = lerp( float2(0.f, 1.f), float2(0.4f, 0.6f), alphaPulseYRange );
        if ( IN.uv.y >= yRange.x && IN.uv.y <= yRange.y )
        {
            float yoff = tr[tri].z;
            
            float zoomAlpha = 1.f - alpha * alpha * alpha;
            float2 txtuv = IN.uv.xy * zoomAlpha + (1.f - zoomAlpha) * 0.5f;
            txtuv.y += yoff;
                        
            float4 txt = texText.Sample( samplerBilinear, txtuv );
            txt.rgb = lerp( txt.rgb, color.rgb, 0.25f );
            
            float alphaPulse = 1 - pow( 2.5 * alpha - 1, 2 );
            color.rgb = lerp( color.rgb, txt.rgb, smoothstep( 0.0, 1.0, txt.a * alphaPulse ) );
        }
    }
    
    



    // varying vignette
    float vI = saturate( 1.0 - length( pow( abs( IN.uv - 0.5 ), 5 * (1.f - fft1) ) )*16.0 );
    
    // fixed vignette
    vI *= pow( 16.0 * IN.uv.x * (1.0 - IN.uv.x) * IN.uv.y * (1.0 - IN.uv.y), 0.4 );
    
    // Add additive flicker
    vI += sqrt( fft8 + fft9 );

    // Set frequency of global effect to 20 variations per second
    float t = float( int( inTime * 15.f ) );

    
    {
        // Add some random lines 
        float fftv = (fft0 + fft1 + fft2 + fft3) / 4.0;
        int l = int( 8.0 * sqrt(fftv) );

        if ( 0 < l ) vI *= randomLine( t + 6.0 + 17.* float( 0 ), IN.uv );
        if ( 1 < l ) vI *= randomLine( t + 6.0 + 17.* float( 1 ), IN.uv );
        if ( 2 < l ) vI *= randomLine( t + 6.0 + 17.* float( 2 ), IN.uv );
        if ( 3 < l ) vI *= randomLine( t + 6.0 + 17.* float( 3 ), IN.uv );
        if ( 4 < l ) vI *= randomLine( t + 6.0 + 17.* float( 4 ), IN.uv );
        if ( 5 < l ) vI *= randomLine( t + 6.0 + 17.* float( 5 ), IN.uv );
        if ( 6 < l ) vI *= randomLine( t + 6.0 + 17.* float( 6 ), IN.uv );
        if ( 7 < l ) vI *= randomLine( t + 6.0 + 17.* float( 7 ), IN.uv );
    }

    {
        // Add some random blotches.
        float fftv = (fft7 + fft8 + fft9 + fft10) / 4.0;
        int s = int( max( 8.0 * sqrt( fftv ) - 2.0, 0.0 ) );

        if ( 0 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 0 ), IN.uv );
        if ( 1 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 1 ), IN.uv );
        if ( 2 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 2 ), IN.uv );
        if ( 3 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 3 ), IN.uv );
        if ( 4 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 4 ), IN.uv );
        if ( 5 < s ) vI *= randomBlotch( t + 6.0 + 19.* float( 5 ), IN.uv );
    }

    //color *= saturate( 1.0 - length( pow( abs( q ), 5 * ( 1.f - fft1 ) ) )*16.0 ); // vignette (kind of)
    color *= vI;
    color.xyz *= (1.0 + (rand( IN.uv + t*.1 ) - .2)*.75);

    color *= smoothstep( 0.f, 1.f, fadeValueInv ); // smoothstep( 0.f, 2.f, inTime );

    return color;
}