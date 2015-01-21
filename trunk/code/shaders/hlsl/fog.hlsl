passes:
{
    fog = 
    {
	    vertex = "vs_screenquad";
        pixel = "ps_fog";
    };

    sky =
    {
        vertex = "vs_screenquad";
        pixel = "ps_sky";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/types.hlsl>

cbuffer MaterialData : register(b3)
{
    float3 _sunDir;
    float3 _sunColor;
    float _sunIlluminance;
    float _skyIlluminance;
    float _fallOff;
    float _fallOffPower;
};

Texture2D _tex_depth : register(t0);
Texture2D _tex_color : register(t1);

SamplerState _sampler : register(s0);

float resolveLinearDepth( float hw_depth, float2 znear_zfar )
{
    float c1 = znear_zfar.y / znear_zfar.x;
    float c0 = 1.0 - c1;
    return 1.0 / (c0 * hw_depth + c1);
}

float3 resolvePositionVS( float2 wpos, float linear_depth, float2 ab_inv )
{
    float2 xy = wpos * ab_inv * -linear_depth;
	return float3( xy, linear_depth );
}

float3 computeSkyColor( in float3 rayDir )
{
    float sunAmount = saturate( dot( rayDir, _sunDir ) );
    const float3 skyColor = float3(0.4f, 0.5f, 1.0f) * _skyIlluminance;
    const float3 sunColor = _sunColor * _sunIlluminance;
    return lerp( skyColor, sunColor, pow( sunAmount, 8.0 ) );
}

float3 applyFog( in float3 color, in float distance, in float3 rayDir )
{
    float3 fogColor = computeSkyColor( rayDir );
    float be = 1.f - exp( -distance * _fallOff*E );
    //float bi = exp( -distance * _fallOffPower*E );
    return lerp( color, fogColor, be );

    //return color*be + fogColor*bi;
}

float4 ps_fog( in out_VS_screenquad input ) : SV_Target0
{
    float hwDepth = _tex_depth.SampleLevel( _sampler, input.uv, 0.f ).r;
    float linDepth = resolveLinearDepth( hwDepth, camera_params.zw );

    float2 winPos = input.wpos01 * 2.0 - 1.0;
    float3 vsPos = resolvePositionVS( winPos, linDepth, proj_params.xy );
    float3 worldPos = mul( camera_world, float4(vsPos,1.f) ).xyz;
    
    float3 ray = worldPos - eye_pos.xyz;
    float len = length( ray );
    float3 rayDir = ray * rcp( len );

    float3 color = _tex_color.SampleLevel( _sampler, input.uv, 0.f ).xyz;
    float3 result = applyFog( color, linDepth, rayDir );
    return float4(result, 1.f);
}

float4 ps_sky( in out_VS_screenquad input ) : SV_Target0
{
    const float2 winPos = input.wpos01 * 2.0 - 1.0;
    float3 vsPos = resolvePositionVS( winPos, 1.f, proj_params.xy );
    const float3 rayDir = normalize( mul( (float3x3)camera_world, vsPos ) );
    const float3 skyColor = computeSkyColor( rayDir );
    
    return float4((float3)skyColor, 1.f);
}