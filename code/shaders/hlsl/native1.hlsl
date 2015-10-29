passes:
{
    color =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        hwstate =
        {
            depth_test = 1;
            depth_write = 0;
            //color_mask = "";
        };
    };
}; #~header

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float3 pos	  	  : POSITION;
    float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float4 s_pos    : TEXCOORD0;
    float3 w_pos	: TEXCOORD1;
    float3 w_normal:TEXCOORD2;
};

struct out_PS
{
    float4 rgba : SV_Target0;
    //float4 albedo : SV_Target1;
};

#include <sys/types.hlsl>
#include <sys/frame_data.hlsl>
#include <sys/material.hlsl>
#include <sys/brdf.hlsl>
#include <sys/lights.hlsl>
#include <sys/vertex_transform.hlsl>

in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld( row0, row1, row2, IN.instanceID );
    fetchWorldIT( row0IT, row1IT, row2IT, IN.instanceID );

    float4 localPos = float4(IN.pos, 1.0);

    OUT.w_pos = transformPosition( row0, row1, row2, localPos );
    OUT.w_normal = transformNormal( row0IT, row1IT, row2IT, IN.normal );
    OUT.h_pos = mul( _camera_viewProj, float4(OUT.w_pos, 1.f) );
    OUT.s_pos = OUT.h_pos;

    return OUT;
}

shared cbuffer MaterialData: register(b3)
{
    MATERIAL_VARIABLES;
};

Texture2D _texSAO : register(t4);
Texture2D _texShadow : register(t5);
SamplerState _samplSAO : register(s4);

out_PS ps_main( in_PS IN )
{
    out_PS OUT;
    float2 screenPos01 = (IN.s_pos.xy / IN.s_pos.w) * 0.5 + 0.5;
    float2 shadowUV = float2(screenPos01.x, 1.0 - screenPos01.y);
    
    ShadingData shd;
    shd.N = normalize( IN.w_normal );
    shd.V = _camera_viewDir.xyz;
    shd.shadow = _texShadow.SampleLevel( _samplSAO, shadowUV, 0.0 ).r;
    shd.ssao = _texSAO.SampleLevel( _samplSAO, shadowUV, 0.0 ).r;
    
    Material mat;
    ASSIGN_MATERIAL_FROM_CBUFFER( mat );
    //float3 c = BRDF( L, shd, mat );
    float3 c = ( float3 )0;
    float sunIlluminance = 0;
    evaluateSunLight( c, sunIlluminance, shd, IN.w_pos, mat );

    float3 a = ( float3 )0;
    float ambientIlluminance = 0;
    evaluateAmbientLight( a, ambientIlluminance, shd, mat );

    c *= sunIlluminance;
    c += a * ambientIlluminance;

    //float aNdotL = -( clamp( dot( shd.N, -_sunDirection ), -mat.ambientCoeff, -1.f + mat.ambientCoeff ) );
    //float3 ambient = aNdotL * mat.diffuseColor * mat.ambientColor;
    //ambient = ( (1.f - ambient ) * ambient );
    //ambient *= mat.ambientCoeff * shd.ssao;
    
    //c = lerp( ambient, c, shd.shadow );
        
    //float3 C = diffuseColor;
    //float NdotL = saturate( dot( N, L ) );

    OUT.rgba = float4( c, 1.0 );
    //OUT.albedo = float4(mat.diffuseColor, 1.0);
    //OUT.rgba = float4( 1.0, 0.0, 0.0, 1.0 );
    return OUT;
}