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
    
    float3 color = bgColor.rgb;
    color += spisColor.rgb * spisColor.a;
    
    //color += lerp( color, color * 500.f, fft0 * bgMask.rgb * 10.f );
    color += fft0 * bgMask.r * 10.f;
    color += fft5 * bgMask.ggb * 10.f;

    color = lerp( color, color*0.1, sqrt( fft ) );

    return float4( color, 1.0 );
}
