passes:
{
    background =
    {
        vertex = "vs_screenquad";
        pixel = "ps_background";
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
SamplerState samplerNearest;
SamplerState samplerLinear;
SamplerState samplerBilinear;


float noise( in float3 x )
{
    float3 p = floor( x );
    float3 f = frac( x );
    f = f*f*(3.0 - 2.0*f);

    float2 uv = (p.xy + float2( 37.0, 17.0 )*p.z) + f.xy;
    float2 rg = texNoise.Sample( samplerBilinear, (uv + 0.5) / 256.0 ).yx;
    return lerp( rg.x, rg.y, f.z );
}

float4 map( float3 p )
{
    float den = 0.2 - p.y;

    // invert space	
    p = -7.0*p / dot( p, p );

    // twist space	
    float co = cos( den - 0.25*inTime );
    float si = sin( den - 0.25*inTime );
    p.xz = mul( float2x2( co, -si, si, co ), p.xz );

    // smoke	
    float f;
    float3 q = p - float3( 0.0, 1.0, 0.0 )*inTime;
    f =  0.50000*noise( q ); q = q*2.02 - float3( 0.0, 1.0, 0.0 )*inTime;
    f += 0.25000*noise( q ); q = q*2.03 - float3( 0.0, 1.0, 0.0 )*inTime;
    f += 0.12500*noise( q ); q = q*2.01 - float3( 0.0, 1.0, 0.0 )*inTime;
    f += 0.06250*noise( q ); q = q*2.02 - float3( 0.0, 1.0, 0.0 )*inTime;
    f += 0.03125*noise( q );

    den = clamp( den + 4.0*f, 0.0, 1.0 );

    float3 col = lerp( float3( 1.0, 0.9, 0.8 ), float3( 0.4, 0.15, 0.1 ), den ) + 0.05*sin( p );

    return float4( col, den );
}

float3 raymarch( in float3 ro, in float3 rd, in float2 pixel )
{
    float4 sum = (float4)( 0.0 );

    float t = 0.0;

    // dithering	
    //t += 0.05*texture2D(iChannel0, pixel.xy / iChannelResolution[0].x).x;
    
    [loop]
    for ( int i = 0; i<64; i++ )
    {
        //if ( sum.a > 0.99 ) 
          //  break;

        float3 pos = ro + t*rd;
        float4 col = map( pos );

        col.xyz *= lerp( 3.1*float3( 1.0, 0.5, 0.05 ), float3( 0.48, 0.53, 0.5 ), clamp( (pos.y - 0.2) / 2.0, 0.0, 1.0 ) );

        col.a *= 0.6;
        col.rgb *= col.a;

        sum = sum + col*(1.0 - sum.a);
        sum.a = saturate( sum.a );


        t += 0.15;
    }

    return clamp( sum.xyz, 0.0, 1.0 );
}

float4 ps_background( in_PS IN ) : SV_Target0
{
    float2 fragCoord = IN.uv.xy * inResolution.xy;
    float2 q = IN.uv.xy;
    float2 p = -1.0 + 2.0*q;
    p.x *= inResolution.x / inResolution.y;
    
    float2 mo = float2( 0, 0 );

    // camera
    float3 ro = 4.0*normalize( float3(( 3.0*mo.x ), 1.4 - 1.0*(mo.y - .1), ( 3.0*mo.x )) );
    float3 ta = float3(0.0, 1.0, 0.0);
    float cr = 0.5*cos( 0.7*inTime );

    // build ray
    float3 ww = normalize( ta - ro );
    float3 uu = normalize( cross( float3(sin( cr ), cos( cr ), 0.0), ww ) );
    float3 vv = normalize( cross( ww, uu ) );
    float3 rd = normalize( p.x*uu + p.y*vv + 2.0*ww );

    float3 col = raymarch( ro, rd, fragCoord );

    //float4 noise = texNoise.Sample( samplerNearest, IN.uv );
    return float4( col, 1 ); // float4(0.0, sin( inTime ) * 0.5f + 0.5f, 0.0, 1.0);
}
