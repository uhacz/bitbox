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

#include <sys/types.hlsl>
#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/util.hlsl>

cbuffer MaterialData : register(b3)
{
    float3 _sunDir;
    float3 _sunColor;
    float3 _skyColor;
    float _sunIlluminance;
    float _skyIlluminance;
    float _fallOff;
};

Texture2D _tex_depth : register(t0);
Texture2D _tex_color : register(t1);
Texture2D _tex_shadow: register(t2);
SamplerState _sampler : register(s0);
SamplerState _sampler_shadow : register(s1);


float3 computeSkyColor( in float3 rayDir )
{
    float sunAmount = saturate( dot( rayDir, _sunDir ) );
    const float3 skyColor = _skyColor * _skyIlluminance;
    const float3 sunColor = _sunColor * _sunIlluminance;
    return lerp( skyColor, sunColor, pow( sunAmount, 64.0 ) );
}

float3 applyFog( in float3 color, in float distance, in float3 rayDir, in float shadow )
{
    float3 fogColor = computeSkyColor( rayDir );
    float3 sunColor = _sunColor * _sunIlluminance;
        
    float be = 1.f - exp( -distance * _fallOff );
    //fogColor = lerp( sunColor, fogColor, smoothstep( 0.0, 0.5, be ) );
    fogColor += sunColor * exp( -distance * _fallOff*32 );
    return lerp( color, fogColor, be * shadow  );
}

float4 ps_fog( in out_VS_screenquad input ) : SV_Target0
{
    float hwDepth = _tex_depth.SampleLevel( _sampler, input.uv, 0.f ).r;

    if( hwDepth >= 1.0 )
    {
        return _tex_color.SampleLevel( _sampler, input.uv, 0.f );
    }

    float linDepth = resolveLinearDepth( hwDepth );
    float2 winPos = input.screenPos;
    float3 vsPos = resolvePositionVS( winPos, -linDepth );
    float3 worldPos = mul( _camera_world, float4(vsPos,1.f) ).xyz;
    //
    float3 ray = worldPos - _camera_eyePos.xyz;
    float len = length( ray );
    float3 rayDir = ray * rcp( len );

    float3 color = _tex_color.SampleLevel( _sampler, input.uv, 0.f ).xyz;
    float shadow = _tex_shadow.SampleLevel( _sampler_shadow, input.uv, 0.f ).x;
    
    float3 result = applyFog( color, linDepth, rayDir, shadow );
    
    return float4(result, 1.f);
    //return float4( color, 1.f );
}

float4 ps_sky( in out_VS_screenquad input ) : SV_Target0
{
    const float2 winPos = input.screenPos;// *2.0 - 1.0;
    float3 vsPos = resolvePositionVS( winPos, _camera_zFar );
    const float3 rayDir = normalize( mul( (float3x3)_camera_world, vsPos ) );
    const float3 skyColor = computeSkyColor( rayDir );
    
    return float4((float3)skyColor, 1.f);
}