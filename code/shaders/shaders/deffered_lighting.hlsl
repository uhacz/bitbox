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
texture2D gbuffer_wpos_rough : register(t1);
texture2D gbuffer_wnrm_metal : register(t2);

shared
cbuffer FrameData : register(BSLOT( SLOT_FRAME_DATA ) )
{
    float3 cameraEye;
    float3 sunColor;
    float sunIntensity;
    float3 vsSunL; // L means direction TO light
};

float3 ps_lighting(in_PS IN) : SV_Target0
{
    const float sun_radius_over_distance = 0.00465;
    float4 albedo_spec = gbuffer_albedo_spec.SampleLevel(_samp_point, IN.uv, 0.0);
    float3 wPos = gbuffer_wpos_rough.SampleLevel(_samp_point, IN.uv, 0.0).rgb;
    float3 wNrm = gbuffer_wnrm_metal.SampleLevel( _samp_point, IN.uv, 0.0).rgb;

    float3 surfaceToCamera = normalize(cameraEye - wPos);
    float3 surfaceToLight = vsSunL;
    float3 r = reflect( surfaceToCamera, wNrm );
    float3 D = dot(surfaceToLight, r) * r - surfaceToLight;
    float3 P = surfaceToLight + D * saturate(sun_radius_over_distance * rsqrt(dot(D, D)));
    float3 specular_sun = normalize(P);

    float3 diffuse_sun = saturate(dot(wNrm, vsSunL)) * sunColor;
    diffuse_sun *= albedo_spec.rgb;

    return float4(wNrm, 1.0);
}