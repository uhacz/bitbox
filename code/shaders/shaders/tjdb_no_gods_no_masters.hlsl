passes:
{
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
SamplerState _sampPoint : register ( s0 );

float4 ps_blit( in_PS IN ) : SV_Target0
{
    return gSrcTexture.SampleLevel( _sampPoint, IN.uv, 0 );
}
