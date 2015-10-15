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
};

#include <sys/types.hlsl>
#include <sys/frame_data.hlsl>
#include <sys/material.hlsl>
#include <sys/brdf.hlsl>
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

out_PS ps_main( in_PS input )
{
    out_PS OUT;
    
    float3 L = normalize( float3(-1.f, 1.f, 1.f) );
    
    ShadingData shd;
    shd.N = normalize( input.w_normal );
    shd.V = _camera_viewDir.xyz;
    shd.shadow = 1;
    shd.ssao = 1;
    
    Material mat;
    ASSIGN_MATERIAL_FROM_CBUFFER( mat );
    float3 c = BRDF( L, shd, mat );
    //float3 C = diffuseColor;
    //float NdotL = saturate( dot( N, L ) );

    OUT.rgba = float4( c, 1.0 );
    //OUT.rgba = float4( 1.0, 0.0, 0.0, 1.0 );
    return OUT;
}