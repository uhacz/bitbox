passes:
{
    background =
    {
        vertex = "vs_screenquad";
        pixel = "ps_background";
    };

    foreground = 
    {
        vertex = "vs_screenquad";
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

float4 ps_background( in_PS IN ) : SV_Target0
{
    return float4(0, 0, 0, 1);
}

float4 ps_foreground( in_PS IN ) : SV_Target0
{
    float4 color = texImage.SampleLevel( samplerNearest, IN.uv, 0.0 );
    return color;
}