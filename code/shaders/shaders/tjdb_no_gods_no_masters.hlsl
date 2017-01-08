passes:
{
    main = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_main";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
    }    

    flame = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_flame";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};        
    }

    blit =
    {
        vertex = "vs_screenquad";
        pixel = "ps_blit";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
    };
}; #~header

#include <sys/vs_screenquad.hlsl>
#include <sys/samplers.hlsl>
#include <sys/binding_map.h>
#include <tjdb_no_gods_no_masters_data.h>

#define in_PS out_VS_screenquad



Texture2D gSrcTexture;

Texture2D gBgTex;
Texture2D gBgMaskTex;
Texture2D gLogoMaskTex;
Texture2D gSpisTex;
Texture2D gFlameTex;
Texture1D<float> gFFTTex;


float4 ps_blit( in_PS IN ) : SV_Target0
{
    return gSrcTexture.SampleLevel( _samp_point, IN.uv, 0 );
}

float3 Overlay( float3 base, float3 blend )
{
    return ( base < 0.5 ? ( 2.0 * base * blend ) : ( 1.0 - 2.0 * ( 1.0 - base ) * ( 1.0 - blend ) ) );
}

#define NUM_SONGS 7
static const float2 g_spisLineSize = float2( 0.28125, 0.0407407407 );
static const float g_spisU = 0.1890625;
static const float g_spisV[NUM_SONGS] =
{
    0.3435185185,
    0.3833333333,
    0.4203703704,
    0.4574074074,
    0.5666666667,
    0.6046296296,
    0.6435185185,
};

float toM11( float v01 )
{
    return v01 * 2 - 1;
}

float3 maskToColor( float4 rgba )
{
    return rgba.rgb * rgba.a;
}

float4 ps_main( in_PS IN ) : SV_Target0
{
    float fft   = gFFTTex.Sample( _samp_linear, IN.uv.x ).r;
    float fft0  = gFFTTex.Sample( _samp_point, 0.0f ).r;
    float fft05 = gFFTTex.Sample( _samp_point, 0.05f ).r;
    float fft1  = gFFTTex.Sample( _samp_point, 0.1f ).r;
    float fft2  = gFFTTex.Sample( _samp_point, 0.2f ).r;
    float fft3  = gFFTTex.Sample( _samp_point, 0.3f ).r;
    float fft4  = gFFTTex.Sample( _samp_point, 0.4f ).r;
    float fft5  = gFFTTex.Sample( _samp_point, 0.5f ).r;
    float fft6  = gFFTTex.Sample( _samp_point, 0.6f ).r;
    float fft7  = gFFTTex.Sample( _samp_point, 0.7f ).r;
    float fft8  = gFFTTex.Sample( _samp_point, 0.8f ).r;
    float fft9  = gFFTTex.Sample( _samp_point, 0.9f ).r;
    float fft10 = gFFTTex.Sample( _samp_point, 1.0f ).r;
    
    float4 flameValue = gFlameTex.Sample( _samp_point, IN.uv, 0 );
    float3 bgMask = gBgMaskTex.SampleLevel( _samp_point, IN.uv, 0 );
    float logoMask = gLogoMaskTex.SampleLevel( _samp_point, IN.uv, 0 ).r;

    float2 uv = IN.uv;
    
    float4 bgColor = gBgTex.SampleLevel( _samp_point, uv, 0 );
    float4 spisColor = gSpisTex.SampleLevel( _samp_point, uv, 0 );
    
    float3 spisColorValue = spisColor.rgb * spisColor.a;
    if( currentSong < NUM_SONGS )
    {
        float songAlpha = songTime / songDuration;
        bool2 inRange;
        inRange.x = IN.uv.x >= g_spisU && IN.uv.x <= ( g_spisU + g_spisLineSize.x );
        inRange.y = IN.uv.y >= g_spisV[currentSong] && IN.uv.y <= ( g_spisV[currentSong] + g_spisLineSize.y );
        if( all( inRange ) )
        {
            float2 offR = float2( 0.f, toM11( fft0 ) ) * resolutionRcp * 2.0;
            float2 offG = float2( toM11( fft05 ), toM11( fft0 ) ) * resolutionRcp * 2.0;
            float2 offB = float2( toM11( fft4 ), 0.f ) * resolutionRcp * 2.0;
            float4 colorValueR = gSpisTex.SampleLevel( _samp_point, IN.uv + offR, 0 );
            float4 colorValueG = gSpisTex.SampleLevel( _samp_point, IN.uv + offG, 0 );
            float4 colorValueB = gSpisTex.SampleLevel( _samp_point, IN.uv + offB, 0 );
            
            float4 colorValue0;
            colorValue0.r = colorValueR.rgb * colorValueR.a;
            colorValue0.g = colorValueG.rgb * colorValueG.a;
            colorValue0.b = colorValueB.rgb * colorValueB.a;

            float3 colorValue1 = maskToColor( gSpisTex.SampleLevel( _samp_point, IN.uv + ( toM11( fft ) * resolutionRcp * 2.0 ), 0 ) );

            float lerpAlpha = smoothstep( 0.f, 1.f, songTime ) * ( 1.f - smoothstep( songDuration - 1.f, songDuration, songTime ) );
            spisColorValue = lerp( spisColorValue, max( colorValue0, colorValue1 ), lerpAlpha );
        }
    }

    float3 color = bgColor.rgb;
    color += spisColorValue;
    
    //color += lerp( color, color * 500.f, fft0 * bgMask.rgb * 10.f );
    color += fft0 * bgMask.r * 10.f;
    color += fft5 * bgMask.ggb * 10.f;

    color = lerp( color, color*0.1, sqrt( fft ) );

    if( flameValue.x > 0.001 )
    {
        //color += smoothstep( 0, flameValue.x * 20, flameValue.x );
        color += pow( flameValue.xxx, 2 ) * 20.f;
        //color += Overlay( color, saturate( flameValue.x ) *10 );
    }

    //if( logoMask > 0.f )
    //{
    //    color *= saturate( 0.2 - fft05 );
    //}

    return float4( color, 1.0 );
    //return flameValue.xxxx;
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
Texture2D<float4> gPrevFlameTex;
Texture2D<float4> gRandomTex;

#define RotNum 5
//#define SUPPORT_EVEN_ROTNUM

#define Res  resolution
#define Res1 float2( 256, 256 )
#define Sampler _samp_bilinear



float4 randS( float2 uv )
{
    return gRandomTex.SampleLevel( Sampler, uv * Res.xy / Res1.xy, 0 ) - 0.5;
}

//float2 getGradBlue( float2 pos )
//{
//    float eps = 1.4;
//    float2 d = float2( eps, 0 );
//    float2 result = float2(
//        gPrevFlameTex.Sample( Sampler, frac( ( pos + d.xy ) / Res.xy ) ).z - gPrevFlameTex.Sample( Sampler, frac( ( pos - d.xy ) / Res.xy ) ).z,
//        gPrevFlameTex.Sample( Sampler, frac( ( pos + d.yx ) / Res.xy ) ).z - gPrevFlameTex.Sample( Sampler, frac( ( pos - d.yx ) / Res.xy ) ).z
//    );
//    return result / ( eps * 2. );
//}

float getRot( float2 pos, float2 b, float2x2 rot2x2 )
{
    float2 p = b;
    float rot = 0.0;
    for( int i = 0; i < RotNum; i++ )
    {
        float2 v = gPrevFlameTex.Sample( Sampler, frac( ( pos + p ) / Res.xy ) ).xy;
        rot += dot( v, p.yx * float2( 1, -1 ) );
        p = mul( rot2x2, p );
    }
    return rot / float( RotNum ) / dot( b, b );
}

float4 getC2( float2 uv )
{
    // line 0 holds writer infos so take 1st line instead
    //if( uv.y * resolution.y < 1. )
    //    uv.y += resolutionRcp.y;
    return gBgMaskTex.Sample( Sampler, uv );
}

float4 ps_flame( in_PS IN ) : SV_Target0
{
    //float3 prevFlameValue = gPrevFlameTex.SampleLevel( _samp_point, IN.uv, 0 );
    //float3 bgMask = gBgMaskTex.SampleLevel( _samp_point, IN.uv, 0 );
    //
    //return bgMask.r;
    
    float fft = gFFTTex.Sample( _samp_point, 0.01f ).r;
    fft += gFFTTex.Sample( _samp_point, 0.25 ).r;
    fft += gFFTTex.Sample( _samp_point, 0.5 ).r;

    float angBias = 1.f;
     //smoothstep( 0, 0.2, fft );
    const float ang = ( 2.0 + angBias ) * 3.1415926535 / float( RotNum );
    const float2x2 m = float2x2( cos( ang ), sin( ang ), -sin( ang ), cos( ang ) );
    const float2x2 mh = float2x2( cos( ang * 0.5 ), sin( ang * 0.5 ), -sin( ang * 0.5 ), cos( ang * 0.5 ) );

    float2 uv = IN.uv; //fragCoord / iResolution.xy;
    float2 pos = uv * resolution;
    float4 rnd4 = randS( float2( float( frameNo ) / Res.x, 0.5 / Res1.y ) );
    float rnd = rnd4.x;
    
    float2 b = float2( cos( ang * rnd ), sin( ang * rnd ) );
    float2 v = float2( 0, 0 );
    float bbMax = 0.7 * Res.y * 1.;
    bbMax *= bbMax;
    for( int l = 0; l < 8; l++ )
    {
        if( dot( b, b ) > bbMax )
            break;
        float2 p = b;
        for( int i = 0; i < RotNum; i++ )
        {
            // this is faster but works only for odd RotNum
            v += p.yx * getRot( pos + p, -mul( mh, b ), m );
            p = mul( m, p );
        }
        b *= 2.0;
    }
    float4 c2 = sqrt( getC2( uv ) ) * smoothstep( 0, 0.1, fft ); //* ( sin( time * 3 ) * 0.5 + 1.0 );
    
    float strength = clamp( 1. - 1. * c2.z, 0., 1. );
    float4 gravity = float4( 0.2, 1.0, 0.2, 0.2 ) + fft * 0.2;
    float4 fragColor = gPrevFlameTex.Sample( Sampler, frac( ( pos + v * strength * ( 2. /*+2.*iMouse.y/Res.y*/ ) * float2( -1, 1 ) * 1.0 ) / Res.xy ) );
    fragColor = lerp( fragColor, c2 * gravity, .3 * clamp( 1. - strength, 0., 1. ) );
    
    // damping
    float dampingStrength = 0.7;//sin( time * 5 ) * 0.5f + 2.f;
    fragColor.xy = lerp( fragColor.xy, float2( 0.0, 0.0 ), deltaTime*dampingStrength );

    return fragColor;
}