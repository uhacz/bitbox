passes:
{
    color =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
}; #~header

#include <sys/types.hlsl>
#include <sys/frame_data.hlsl>
#include <sys/instance_data.hlsl>
#include <sys/material.hlsl>
#include <sys/brdf.hlsl>
#include <sys/lights.hlsl>

shared cbuffer MaterialData: register(b3)
{
    MATERIAL_VARIABLES;
};

struct in_VS
{
	uint   instanceID : SV_InstanceID;
	float4 pos	  	  : POSITION;
	float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float4 s_pos    : TEXCOORD0;
	float3 w_pos	: TEXCOORD1;
	/*nointerpolation*/ float3 w_normal:TEXCOORD2;
};

struct out_PS
{
	float4 rgba : SV_Target0;
};

Texture2D _shadowMap : register(t3);
Texture2D _ssaoMap : register(t4);

SamplerState _samplShadowMap : register(s3);
SamplerState _samplSsaoMap : register(s4);

in_PS vs_main( in_VS input )
{
    in_PS output;
	const matrix wm = world_matrix[input.instanceID];
	const matrix wmit = world_it_matrix[input.instanceID];
	const float3 world_pos = mul( wm, input.pos ).xyz;
    
    const float4 hpos = mul( _camera_viewProj, float4( world_pos, 1.f ) );
    
    output.h_pos = hpos;
	output.w_pos = world_pos.xyz;
    output.w_normal = mul( (float3x3)wmit, input.normal );
    output.s_pos = hpos;
    return output;
}

out_PS ps_main( in_PS input )
{
	out_PS OUT;
    OUT.rgba = float4( 0.0, 0.0, 0.0, 1.0 );

    const float2 screenPos01 = (input.s_pos.xy / input.s_pos.w ) * 0.5 + 0.5;
    uint2 tileXY = computeTileXY( screenPos01, _numTilesXY, _renderTarget_size, _tileSizeRcp );
    uint tileIdx = _numTilesXY.x * tileXY.y + tileXY.x;
    uint lightsIndexBegin = (tileIdx)* _maxLights;

    Material mat;
    ASSIGN_MATERIAL_FROM_CBUFFER( mat );

    const float3 N = normalize( input.w_normal );
    const float3 V = -_camera_viewDir.xyz;
    float2 shadowUV = float2(screenPos01.x, 1.0 - screenPos01.y);
    float shadowValue = _shadowMap.SampleLevel( _samplShadowMap, shadowUV, 0.f ).r;
    float ssaoValue = _ssaoMap.SampleLevel( _samplSsaoMap, shadowUV, 0.f ).r;

    ShadingData shData;
    shData.N = N;
    shData.V = V;
    
    float3 colorFromLights = float3(0.f, 0.f, 0.f);
    
    uint pointLightIndex = lightsIndexBegin;
    uint pointLightDataIndex = _lightsIndices[pointLightIndex] & 0xFFFF;
    
    while( pointLightDataIndex != 0xFFFF )
    {
        float4 lightPosRad = _lightsData[pointLightDataIndex * 2];
        float4 lightColInt = _lightsData[pointLightDataIndex * 2 + 1];

        const float3 unormalizedLightVector = lightPosRad.xyz - input.w_pos;
        const float distanceToLight = length( unormalizedLightVector );
        const float3 L = normalize( unormalizedLightVector );
        const float att = getDistanceAtt( unormalizedLightVector, 1.f / (lightPosRad.w*lightPosRad.w) );
        
        float3 brdf = BRDF( L, shData, mat );
        colorFromLights += brdf * att * lightColInt.xyz * lightColInt.w;
        
        pointLightIndex++;
        pointLightDataIndex = _lightsIndices[pointLightIndex] & 0xFFFF;
    }

    float3 sunIlluminance = evaluateSunLight( shData, input.w_pos, mat );
    colorFromLights += _sunColor * sunIlluminance;

    float colorInShadow = lerp( 0.1f, 0.5f, ssaoValue );
    float colorCoeff = lerp( colorInShadow, 1.f, shadowValue );
    OUT.rgba.xyz = colorFromLights * colorCoeff;// lerp( colorFromLights * colorInShadow, colorFromLights, shadowValue );
    //OUT.rgba.xyz = colorFromLights;

    //float3 tmpColors[] =
    //{
    //    float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3(0.f, 0.f, 1.f),
    //    float3(1.f, 1.f, 0.f), float3(0.f, 1.f, 1.f), float3(0.1f, 0.1f, 0.1f),
    //};

    //OUT.rgba.xyz *= tmpColors[( tileIdx + tileXY.y) % 6] * 0.2f;
    //OUT.rgba.xyz = screenPos01.xyx;
    //OUT.rgba.z = 0.f;
    return OUT;
}
