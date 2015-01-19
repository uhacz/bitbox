passes:
{
    fog = 
    {
	    vertex = "vs_screenquad";
        pixel = "ps_fog";
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>

cbuffer MaterialData : register(b3)
{
    
};

Texture2D _tex_depth : register(t0);
Texture2D _tex_color : register(t1);

SamplerState _sampler : register(s0);

float3 resolvePositionVS( float2 wpos, float linear_depth, float2 ab_inv )
{
    float2 xy = wpos * ab_inv * -linear_depth;
	return float3( xy, linear_depth );
}

float4 ps_fog( in out_VS_screenquad input ) : SV_Target0
{
    float hwDepth = _tex_depth.SampleLevel( _sampler, input.uv, 0.f ).r;
    float linDepth = -( proj_params.w / ( hwDepth + proj_params.z ) );

    float2 winPos = input.wpos01 * 2.0 - 1.0;
    float3 vsPos = resolvePositionVS( winPos, linDepth, proj_params.xy );
    float3 worldPos = mul( camera_world, float4(vsPos,1.f) ).xyz;
    


    return float4( worldPos * 1000, 1.f );
}