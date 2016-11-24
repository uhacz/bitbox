passes:
{
    lighting = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_lighting";
    };

}; #~header

#include <sys/samplers.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/binding_map.h>

#define in_PS out_VS_screenquad

texture2D gbuffer_albedo_spec : register(t0);
texture2D gbuffer_vpos_rough : register(t1);
texture2D gbuffer_vnrm_metal : register(t2);

shared
cbuffer FrameData : register(BSLOT( SLOT_FRAME_DATA ) )
{
    float3 sunColor;
    float sunIntensity;
    float3 vsSunL; // L means direction TO light
};

float3 ps_lighting(in_PS IN) : SV_Target0
{
    const float sun_radius_over_distance = 0.00465;
    float4 albedo_spec = gbuffer_albedo_spec.SampleLevel(_samp_point, IN.uv, 0.0);
    float3 vsPosition = gbuffer_vpos_rough.SampleLevel(_samp_point, IN.uv, 0.0).rgb;
    float3 vsNormal = gbuffer_vnrm_metal.SampleLevel( _samp_point, IN.uv, 0.0).rgb;

    float3 r = reflect( -vsPosition, vsNormal);
    float3 D = dot(vsSunL, r) * r - vsSunL;
    float3 P = vsSunL + D * saturate(sun_radius_over_distance * rsqrt(dot(D, D)) );
    float3 specular_sun = normalize(P);

    float3 diffuse_sun = saturate(dot(-vsNormal, vsSunL)) * sunColor;
    //diffuse_sun *= albedo_spec.rgb;

    return float4( diffuse_sun, 1.0);
}