passes:
{
    geometry_notexture =
    {
        vertex = "vs_geometry_main";
        pixel = "ps_geometry_main";
    };
    geometry_texture =
    {
        vertex = "vs_geometry_main";
        pixel = "ps_geometry_main";
        define = 
        {
            USE_TEXTURES = 1;
        };
    };
}; #~header

#include <sys/vertex_transform.hlsl>
#include <sys/material.hlsl>
#include <sys/samplers.hlsl>
struct in_VS
{
    uint instanceID : SV_InstanceID;
    float3 pos : POSITION;
    float3 normal : NORMAL;
#if defined(USE_TEXTURES)
    float2 texcoord : TEXCOORD0;
#endif
}; 


struct in_PS
{
    float4 hpos : SV_Position;
    float3 vpos : TEXCOORD0;
    float3 vnrm : TEXCOORD1;
#if defined(USE_TEXTURES)
    float2 texcoord : TEXCOORD2;
#endif
};

struct out_PS
{
    float4 albedo_spec  : SV_Target0;
    float4 vpos_rough   : SV_Target1;
    float4 vnrm_metal   : SV_Target2;
};

shared
cbuffer FrameData : register( BSLOT( SLOT_FRAME_DATA ) )
{
    matrix _view;
    matrix _viewProj;
};

shared
cbuffer MaterialData : register( BSLOT( SLOT_MATERIAL_DATA ) )
{
    MATERIAL_VARIABLES;
#if defined( USE_TEXTURES )
    MATERIAL_TEXTURES;
#endif
}; 

in_PS vs_geometry_main(in_VS IN)
{
    in_PS OUT = (in_PS) 0;
    
    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld(row0, row1, row2, IN.instanceID);
    fetchWorldIT(row0IT, row1IT, row2IT, IN.instanceID);

    float4 localPos = float4(IN.pos, 1.0);
    float3 wpos = transformPosition(row0, row1, row2, localPos);
    float3 wnrm = transformNormal(row0IT, row1IT, row2IT, IN.normal);

    float4 wpos4 = float4( wpos, 1.0 );
    OUT.hpos = mul( _viewProj, wpos4 );
    OUT.vpos = mul( _view, wpos4 ).xyz;
    OUT.vnrm = mul( (float3x3) _view, wnrm );
#if defined(USE_TEXTURES)
    OUT.texcoord = IN.texcoord;
#endif
    return OUT;
}

out_PS ps_geometry_main( in_PS IN )
{
    float3 albedo_value = diffuse_color;
    albedo_value *= diffuse;
    float specular_value = specular;
    float roughness_value = roughness;
    float metallic_value = metallic;

#if defined( USE_TEXTURES )
    float2 uv = IN.texcoord;
    albedo_value *= diffuse_tex.Sample( _samp_trilinear, uv ).rgb;
    specular_value *= specular_tex.Sample( _samp_point, uv ).r;
    roughness_value *= roughness_tex.Sample( _samp_point, uv ).r;
    metallic_value *= metallic_tex.Sample( _samp_point, uv ).r;
#endif

    float3 vN = normalize( IN.vnrm );

    out_PS OUT = (out_PS)0;
    OUT.albedo_spec = float4( albedo_value.rbg, specular_value );
    OUT.vpos_rough = float4( IN.vpos, roughness_value );
    OUT.vnrm_metal = float4( vN, metallic_value );

    return OUT;
}



