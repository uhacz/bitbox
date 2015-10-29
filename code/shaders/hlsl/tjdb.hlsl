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
SamplerState samplerTrilinear;

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

    float fft   = texFFT.Sample( samplerNearest, IN.uv.x ).r;
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

    float4 maskHi = texMaskHi.Sample( samplerTrilinear, imgUV );
    float4 maskLo = texMaskLo.Sample( samplerTrilinear, float2( imgUV.x + sqrt( fft0 ) * sin( inTime * 10.f + imgUV.y * 55.f ) * 0.001f, imgUV.y ) );

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

        float imgG = texImage.SampleLevel( samplerTrilinear, imgUV, 0.0 ).g;
        float imgR = texImage.SampleLevel( samplerTrilinear, imgUV + offR * 0.1f * sqrt( maskHi.r ), 0.0 ).r;
        float imgB = texImage.SampleLevel( samplerTrilinear, imgUV + offB * 0.1f * sqrt( maskHi.b ), 0.0 ).b;

        float4 logo = texLogo.SampleLevel( samplerTrilinear, imgUV - (offR + offB) * 0.01f, 0.0 );
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

void showText( inout float4 color, in float beginTime, in float endTime, in float2 uv )
{
    const float interval = 5.0;
    if ( inTime >= beginTime && inTime < endTime )
    {
        float3 tr[4];
        tr[0] = float3(beginTime, beginTime + interval, -0.3);
        tr[1] = float3(tr[0].y, tr[0].y + interval, -0.1);
        tr[2] = float3(tr[1].y, tr[1].y + interval,  0.1);
        tr[3] = float3(tr[2].y, tr[2].y + interval,  0.3);

        //{
        //    float3(5.0 , 10.0,-0.3),
        //    float3(10.0, 15.0,-0.1),
        //    float3(15.0, 20.0, 0.1),
        //    float3(20.0, 25.0, 0.3),
        //};

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
        if ( uv.y >= yRange.x && uv.y <= yRange.y )
        {
            float yoff = tr[tri].z;
        
            float zoomAlpha = 1.f - alpha * alpha * alpha;
            float2 txtuv = uv.xy * zoomAlpha + (1.f - zoomAlpha) * 0.5f;
            txtuv.y += yoff;
                    
            float4 txt = texText.Sample( samplerBilinear, txtuv );
            txt.rgb = lerp( txt.rgb, color.rgb, 0.25f );
        
            float alphaPulse = 1 - pow( 2.5 * alpha - 1, 2 );
            color.rgb = lerp( color.rgb, txt.rgb, smoothstep( 0.0, 1.0, txt.a * alphaPulse ) );
        }
    }
}

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
    
    //// timeline
    float x = inTime;
    float brightness = 1.f;
    float brightnessLogo = 0.f;
    float shakeStrength = 1.f;
    float zoom = 1.f;
    float2 target = float2( 0, 0 );
    if( x < 2.f )
    {
        zoom += smoothstep( 1.2f, 1.4f, x );
        target -= smoothstep( 1.2f, 1.4f, x ) * 0.45f;
    }
    else if( x >= 2.f && x < 18.f )
    {
        float a = smoothstep( 2.f, 15.f, x );
        zoom += 1.f - a;
        target = -(1.0 - a) * 0.45f;
    }
    else if( x > 18.f && x < 36.f )
    {
        zoom += smoothstep( 18.f, 32.f, x ) * 0.8f;
        target.x = linearstep( 18.f, 30.f, x ) * 0.2f;
        target.y =-linearstep( 18.f, 30.f, x ) * 0.4f;
        
        brightness = max( 0.15f, 1.f - smoothstep( 25.f, 35.f, x ) );
    }
    else if( x >= 36.f && x < 36.2f )
    {
        float a = smoothstep( 36.1f, 36.2f, x );
        zoom += 1.f - a;
        target = (1.f - a);
        brightness = smoothstep( 36.1f, 36.3f, x );
    }
    else if( x >= 36.2 && x < 53.8f )
    {
        zoom += ((fft0 + fft05) * 0.5f) * 0.05f;
        target += float2(fft8, fft10) * 0.01f;
    }
    else if( x >= 53.8f && x < 71.f )
    {
        float a = smoothstep( 53.8, 54, x );
        zoom += a;
        target.y += a * 0.4;
        target.x -= a * 0.4;

        zoom += ((fft0 + fft05) * 0.5f) * 0.05f * a;
        target += float2(fft8, fft10) * 0.01f * a;

        if( x > 55.2 && x < 58.1)
        {
            target.x += smoothstep( 55.2, 58.1, x ) * 0.6;
        }
        else if( x >= 58.1 && x < 59 )
        {
            target.x += 0.6f;
            target.y -= smoothstep( 58.1, 58.2, x ) * 0.8;
        }
        else if( x >= 59 && x < 71 )
        {
            float b = smoothstep( 59, 71, x );
            target.x = lerp( target.x + 0.6f, 0, b );
            target.y -= 0.8;
            zoom = lerp( zoom, 1.f, b );
        }
    }
    else if( x >= 71 && x < 88.7 )
    {
        float a = smoothstep( 71, 72, x ) * (1.f - smoothstep( 88.6, 88.7, x ) );
        
        float2 t = float2(inTime.xx * 0.001f);// +(fft0 * fft10);
        float f = fft0 + fft1 + fft2 + fft3 + fft4 + fft5 + fft6 + fft7 + fft8 + fft9;
        f *= 0.0001f;

        float zoomA = texNoise.Sample( samplerBilinear, f + t ).r; // ((1 - pow( cos( x*0.25 ), 8 )) * exp( -0.2*(x % 12.55) )) * 2;
        zoom = lerp( 1.f, 2.0f, saturate( zoomA ) * a );


        target.y = texNoise.Sample( samplerTrilinear, 0.2 + t + sin( t * 0.2 ) ).r; // * 0.25 * a;
        target.x = texNoise.Sample( samplerTrilinear, 0.1 - t - cos( t * 0.1 ) ).r; // * 0.25 * a;
        target = target * 2.0 - 1.0;
        target *= a * 0.25f;

        float b = saturate( (fft1 + fft2 + fft3) ) * a;
        target = lerp( target, target * 0.9f, b );
        zoom = lerp( zoom, min( 2.0, zoom + 0.2 ), b );

        shakeStrength = 2.f;


        //float2 t = float2(inTime.xx * 0.001f);// +(fft0 * fft10);
        //float f = fft0 + fft1 + fft2 + fft3 + fft4 + fft5 + fft6 + fft7 + fft8 + fft9;
        //f *= 0.0001f;

        //float zoomA = texNoise.Sample( samplerBilinear, f + t ).r; // ((1 - pow( cos( x*0.25 ), 8 )) * exp( -0.2*(x % 12.55) )) * 2;
        //zoom = lerp( 1.f, 1.5f, saturate( zoomA ) * a );
        //
        //
        //target.y = texNoise.Sample( samplerBilinear, 0.2 + t + sin( t * 0.2 )).r * a * 0.8;
        //target.x = texNoise.Sample( samplerBilinear, 0.1 - t - cos( t * 0.1 )).r * a * 0.8;
        //target = target * 2.0 - 1.0;
        //
        //
        //shakeStrength = a * 2.f * fft05;
        //target.x = smoothstep( -1.f, 1.f, sin( -inTime * 0.5f ) ) * 0.3;
        //target.y = smoothstep( -1.f, 1.f, sin( sin( inTime * 0.25 ) - cos( cos( inTime ) * 0.25 ) ) ) * 0.3;
    }
    else if( x >= 88.7 && x < 106.15 )
    {
        float a = smoothstep( 88.7, 89, x );
        
        brightness += (fft0 + fft05 + fft8) * 5;
        shakeStrength = 2.f;
    }
    else if( x >= 106.15 && x < 123.35 )
    {
        float a = smoothstep( 106.15, 106.25, x );
        zoom += a;
        target.y -= a * 0.45f;
        target.x -= a * 0.45f;

        zoom += ((fft0 + fft05) * 0.5f) * 0.05f * a;
        target += float2(fft8, fft10) * 0.01f * a;

        if ( x > 107.55 && x < 110.45 )
        {
            //target.x += smoothstep( 107.55, 110.45, x ) * 0.3;
            target.y += smoothstep( 107.55, 110.45, x ) * 0.9;
        }
        else if ( x >= 110.45 && x < 110.55 )
        {
            target.x += smoothstep( 110.45, 110.55, x ) * 0.8;
            target.y -= smoothstep( 110.45, 110.55, x ) * 0.9;
        }
        else if ( x >= 110.55 && x < 123.35 )
        {
            float b = smoothstep( 110.55, 123.35, x );
            target.x = lerp( target.x + 0.8f, 0, b );
            target.y = lerp( target.y, 0, b );
            zoom = lerp( zoom, 1.f, b );

            if ( x >= 114.8 )
            {
                float b = smoothstep( 114.8, 115, x ) * ( 1.f - smoothstep( 123.25, 123.35, x ) );
                brightness += ( fft0 + fft1 )* 5.f * b;
            }
        }
    }
    else if( x >= 123.35 && x < 142 )
    {
        brightnessLogo += ( fft1 + fft2 + fft3 ) * 2.f;

        float a = smoothstep( 123.35, 124.35, x ) * ( (1.f - smoothstep( 141, 142, x ) * fft0 ));

        float2 t = float2(inTime.xx * 0.001f);// +(fft0 * fft10);
        float f = fft0 + fft1 + fft2 + fft3 + fft4 + fft5 + fft6 + fft7 + fft8 + fft9;
        f *= 0.0001f;

        float zoomA = texNoise.Sample( samplerTrilinear, f + t ).r; // ((1 - pow( cos( x*0.25 ), 8 )) * exp( -0.2*(x % 12.55) )) * 2;
        zoom = lerp( 1.f, 2.0f, saturate( zoomA ) * a );


        target.y = texNoise.Sample( samplerTrilinear, 0.2 + t + sin( t * 0.2 ) ).r; // * 0.25 * a;
        target.x = texNoise.Sample( samplerTrilinear, 0.1 - t - cos( t * 0.1 ) ).r; // * 0.25 * a;
        target = target * 2.0 - 1.0;
        target *= a * 0.25f;

        float b = saturate( (fft1 + fft2 + fft3) ) * a;
        target = lerp( target, target * 0.9f, b );
        zoom = lerp( zoom, min( 2.0, zoom + 0.2 ), b );

        shakeStrength = 2.f;

    }
    else if( x >= 147.3 && x < 200 )
    {
        float a = smoothstep( 147.3, 147.4, x );
        float aa = linearstep( 147.3, 154.7, x ) * ( 1.f - linearstep( 162, 180, x ) );

        target = float2(0., -0.5 * aa );
        zoom += ( aa * 0.5f ) + saturate( (fft0 + fft1) * 0.25 ) * a * 0.5;

        if( x >= 147.3 && x < 149 )
        {
            a *= (1.f - smoothstep( 148.9, 149, x ) );
        }
        else if( x >= 153.9 && x < 154.7 )
        {
            a = smoothstep( 153.9, 154, x ) * (1.f - smoothstep( 154.6, 154.7, x ));
        }
        else if( x >= 154.7 )
        {
            brightness += fft0*3;
            if( x >= 162 && x < 180.8 )
            {
                brightness += ( fft3 + fft1 ) * 17;
                a *= 0.5f;
            }
            else if ( x >= 180.8 )
            {
                float b = smoothstep( 180.8, 181, x );
                target = lerp( target, float2(-0.45, -0.5), b );
                zoom = lerp( zoom, 1.8f, b );
                brightnessLogo += ( fft0 ) * 10.f;
            }
        }
        else
        {
            a = 0.1f;
        }
        shakeStrength = a * 0.25f;
    }

    //float zoomA = ((1 - pow( cos( x*0.25 ), 8 )) * exp( -0.2*(x % 12.55) )) * 2;
    //float zoom = lerp( 1.f, 1.5f, saturate( zoomA ) );
    //target.x = smoothstep( -1.f, 1.f, sin( -inTime * 0.5f ) ) * 0.3;
    //target.y = smoothstep( -1.f, 1.f, sin( sin( inTime * 0.25 ) - cos( cos( inTime ) * 0.25 ) ) ) * 0.3;

    //float zoom = 2.f - smoothstep( 0.f, 30.f, inTime ) * 0.3f;
    //float2 target = float2(0.05f, 0.025f);

    
    float targetStrength = linearstep( 1.f, 2.f, zoom );
    target = lerp( float2( 0, 0 ), target, ( targetStrength ) );

    float fft0s  = ( fft0 - 0.5 ) * 2.0;
    float fft5s  = ( fft5 - 0.5 ) * 2.0;
    float2 uv = IN.uv * 2.0 - 1.0f;
    uv = ( uv / zoom + target) + (float2(fft0s, fft5s) * (fft10 + fft0)) * 0.025 * shakeStrength;
    uv = uv * 0.5 + 0.5;

    float4 logo = texLogo.SampleLevel( samplerTrilinear, uv, 0.0 );

    float4 color = texBackground.SampleLevel( samplerTrilinear, uv, 0.0 );
    color *= brightness;
    color.rgb += logo.rgb * brightnessLogo * logo.a;
    
    showText( color, 5.0, 25.0, IN.uv );
    showText( color, 71.0, 96.0, IN.uv );

    /// text
    //if ( inTime >= 5.0 && inTime < 25.f )
    //{
    //    float3 tr[] =
    //    {
    //        float3(5.0 , 10.0,-0.3),
    //        float3(10.0, 15.0,-0.1),
    //        float3(15.0, 20.0, 0.1),
    //        float3(20.0, 25.0, 0.3),
    //    };

    //    uint tri = 0;
    //    for ( uint i = 0; i < 4; i++ )
    //    {
    //        if ( inTime >= tr[i].x && inTime < tr[i].y )
    //        {
    //            tri = i;
    //            break;
    //        }
    //    }

    //    float alpha = linearstep( tr[tri].x, tr[tri].y, inTime );
    //    float alphaPulseYRange = 1.f - alpha * alpha * alpha * alpha * alpha;
    //    
    //    float2 yRange = lerp( float2(0.f, 1.f), float2(0.4f, 0.6f), alphaPulseYRange );
    //    if ( IN.uv.y >= yRange.x && IN.uv.y <= yRange.y )
    //    {
    //        float yoff = tr[tri].z;
    //        
    //        float zoomAlpha = 1.f - alpha * alpha * alpha;
    //        float2 txtuv = IN.uv.xy * zoomAlpha + (1.f - zoomAlpha) * 0.5f;
    //        txtuv.y += yoff;
    //                    
    //        float4 txt = texText.Sample( samplerBilinear, txtuv );
    //        txt.rgb = lerp( txt.rgb, color.rgb, 0.25f );
    //        
    //        float alphaPulse = 1 - pow( 2.5 * alpha - 1, 2 );
    //        color.rgb = lerp( color.rgb, txt.rgb, smoothstep( 0.0, 1.0, txt.a * alphaPulse ) );
    //    }
    //}
    
    



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

    float grainStrength = 96.0;

    {
        float x = (IN.uv.x + 4.0) * (IN.uv.y + 4.0) * (inTime * 10.0);
        float grain = (fmod( (fmod( x, 13.0 ) + 1.0) * (fmod( x, 123.0 ) + 1.0), 0.01 ) - 0.005) * grainStrength;
        color.xyz *= saturate(1.0 - grain);
        //color.xyz *= (1.0 + (rand( IN.uv + t*.0001 ) - .2)*0.75);
    }

    color *= smoothstep( 0.f, 1.f, fadeValueInv ); // smoothstep( 0.f, 2.f, inTime );

    return color;
}