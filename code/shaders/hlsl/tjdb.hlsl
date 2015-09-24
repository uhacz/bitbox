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
SamplerState samplerNearest;
SamplerState samplerLinear;
SamplerState samplerBilinear;

// rendering params
//static const float sphsize = 0.5; // planet size
static const float3 boxsize = float3( 0.9f, 0.9 / 1.9433, 0.9 ); // planet size
static const float dist = .27; // distance for glow and distortion
static const float perturb = .3; // distortion amount of the flow around the planet
static const float displacement = .015; // hot air effect
static const float windspeed = .4; // speed of wind flow
static const float steps = 128.; // number of steps for the volumetric rendering
static const float stepsize = .025;
static const float brightness = .43;
static const float3 planetcolor = float3( 0.55, 0.4, 0.3 );
static const float fade = .005; //fade by distance
static const float glow = 3.5; // glow amount, mainly on hit side

// fractal params
static const int iterations = 13;
static const float fractparam = .7;
static const float3 offset = float3( 1.5, 2., -1.5 );

float wind( float3 p )
{
    float box = length( max( abs( p ) - boxsize, 0.0 ) );
    float d = max( 0., dist - max( 0., box ) / boxsize ) / dist; // for distortion and glow area
    float x = max( 0.2, p.y*2. ); // to increase glow on left side
    //p.x *= 1. + max( 0., -p.x - sphsize*.25 )*1.5; // left side distortion (cheesy)
    p -= d*normalize( p )*perturb; // spheric distortion of flow
    p += float3( 0., inTime*windspeed, 0. ); // flow movement
    p = abs( frac( ( p + offset )*.1 ) - .5 ); // tile folding 
    for( int i = 0; i<iterations; i++ )
    {
        p = abs( p ) / dot( p, p ) - fractparam; // the magic formula for the hot flow
    }
    return length( p )*( 1. + d*glow*x ) + d*glow*x; // return the result with glow applied
}


float4 ps_background( in_PS IN ) : SV_Target0
{
    float2 uv = IN.uv - 0.5f;

    float3 dir = float3( uv, 1. );
    dir.x *= inResolution.x / inResolution.y;
    float3 from = float3( 0., 0., -2. + texNoise.Sample( samplerLinear, uv*.5 + inTime ).x*stepsize ); //from+dither

        // volumetric rendering
    float v = 0., l = -0.0001, t = inTime*windspeed*.2;
    for( float r = 10.; r<steps; r++ )
    {
        float3 p = from + r*dir*stepsize;

        float tx = texNoise.Sample( samplerLinear, uv*.2 + float2( t, t ) ).x*displacement; // hot air effect
        float box = length( max( abs( p ) - boxsize, 0.0 ) ) - tx;
        if( box > 0.0 )
        {
            // outside planet, accumulate values as ray goes, applying distance fading
            v += min( 50., wind( p ) )*max( 0., 1. - r*fade );
        }
        else if( l<0. )
        {
            //inside planet, get planet shading if not already 
            //loop continues because of previous problems with breaks and not always optimizes much
            float4 img = texImage.SampleLevel( samplerBilinear, IN.uv, 0.0 );
            l = pow( max( .53, dot( normalize( p ), normalize( float3( 0., .5, -0.3 ) ) ) ), 4. ) * ( img * 2.5 );
        }
    }
    v /= steps; v *= brightness; // average values and apply bright factor
    float3 col = float3( v*1.25, v*v, v*v*v ) + l*planetcolor; // set color
    //col *= 1. - length( pow( abs( uv ), float2( 5., 5. ) ) )*26.; // vignette (kind of)
    float4 color = float4( pow( col, 2.2 ), 1.0 );

    //float4 color = texImage.SampleLevel( samplerBilinear, IN.uv, 0.0 );
    return color;
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
    float scale = 0.6f;
        
    float3 hpos = IN_pos.xyz * float3( scale, scale, 1.0 );
    OUT_hpos = float4( hpos, 1.0 );

    out_VS_screenquad output;
    output.screenPos = hpos.xy;
    output.uv = IN_uv.xy;
    output.uv.y = 1.0 - output.uv.y;
    return output;
}



float4 ps_foreground( out_VS_foreground IN ) : SV_Target0
{
    float4 color = texImage.SampleLevel( samplerBilinear, IN.uv, 0.0 );
    return color;
}