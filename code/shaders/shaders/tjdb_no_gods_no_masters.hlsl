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
//Texture2D gLogoTex;
Texture2D gSpisTex;
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
    
    float4 bgColor = gBgTex.SampleLevel( _samp_point, IN.uv, 0 );
    float3 bgMask = gBgMaskTex.SampleLevel( _samp_point, IN.uv, 0 );
    float4 spisColor = gSpisTex.SampleLevel( _samp_point, IN.uv, 0 );
    
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

    return float4( color, 1.0 );
}
