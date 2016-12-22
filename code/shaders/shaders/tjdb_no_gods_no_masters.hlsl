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

#define in_PS out_VS_screenquad

Texture2D gSrcTexture;

Texture2D gBgTex;
Texture2D gLogoTex;
Texture2D gSpisTex;

SamplerState _sampPoint : register ( s0 );

float4 ps_blit( in_PS IN ) : SV_Target0
{
    return gSrcTexture.SampleLevel( _sampPoint, IN.uv, 0 );
}

float4 ps_main( in_PS IN ) : SV_Target0
{
    float4 bgColor = gBgTex.SampleLevel( _sampPoint, IN.uv, 0 );
    float4 logoColor = gLogoTex.SampleLevel( _sampPoint, IN.uv, 0 );
    float4 spisColor = gSpisTex.SampleLevel( _sampPoint, IN.uv, 0 );
        
    float3 logo = logoColor.rgb * logoColor.a;
    float3 color = lerp( bgColor.rgb, logo, logoColor.a );

    return float4(  color, 1.0 );
}
