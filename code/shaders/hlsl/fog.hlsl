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
    float3 _sunDir;
    float3 _sunColor;
    float _sunIlluminance;
    float _skyIlluminance;
    float _fallOffExt;
    float _fallOffIns;
};

Texture2D _tex_depth : register(t0);
Texture2D _tex_color : register(t1);

SamplerState _sampler : register(s0);

float3 resolvePositionVS( float2 wpos, float linear_depth, float2 ab_inv )
{
    float2 xy = wpos * ab_inv * -linear_depth;
	return float3( xy, linear_depth );
}

float3 applyFog( in float3 color, in float distance, in float3 rayDir )
{
    const float3 skyColor = float3( 0.4f, 0.5f, 1.0f ) * _skyIlluminance;
    const float3 sunColor = _sunColor * _sunIlluminance;

    //float alpha = ;

    //float fogAmount = 1.0f - alpha;
    float sunAmount = saturate( dot( rayDir, _sunDir ) );
    float3 fogColor = lerp( skyColor, sunColor, pow( sunAmount, 8.0 ) );

    float extinction = exp( -distance * _fallOffExt );
    float inscattering = exp( -distance * _fallOffIns );

    return color*(1.f - extinction) + fogColor*(inscattering);
    //return lerp( color, fogColor, extinction );




    //return lerp( color, fogColor, fogAmount );
}

float4 ps_fog( in out_VS_screenquad input ) : SV_Target0
{
    float hwDepth = _tex_depth.SampleLevel( _sampler, input.uv, 0.f ).r;
    float linDepth = -( proj_params.w / ( hwDepth + proj_params.z ) );

    float2 winPos = input.wpos01 * 2.0 - 1.0;
    float3 vsPos = resolvePositionVS( winPos, linDepth, proj_params.xy );
    float3 worldPos = mul( camera_world, float4(vsPos,1.f) ).xyz;
    
    float3 ray = worldPos - eye_pos.xyz;
    float len = length( ray );
    float3 rayDir = ray * rcp( len );
    
    float3 color = _tex_color.SampleLevel( _sampler, input.uv, 0.f );
    float3 result = applyFog( color, len, rayDir );

    return float4( result, 1.f );
}