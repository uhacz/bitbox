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

float4 ps_background( in_PS IN ) : SV_Target0
{
    return float4( 0.0, sin( inTime ) * 0.5f + 0.5f, 0.0, 1.0 );
}
