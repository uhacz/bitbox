passes:
{
    simple =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
    shadow = 
    {
        vertex = "vs_shadow_main";
        pixel = "ps_shadow_main";
    };
}; #~header

#include <sys/vertex_transform.hlsl>
#include <sys/samplers.hlsl>
#include <material_data.h>

Buffer<float3> _particle_data : register(TSLOT(SLOT_INSTANCE_PARTICLE_DATA));

struct in_VS
{
    uint instanceID : SV_InstanceID;
    uint vertexID : SV_VertexID;
};

struct in_PS
{
    float4 hpos : SV_Position;
    float3 wpos : TEXCOORD0;
    float2 texcoord : TEXCOORD2;
};

struct out_PS
{
    float4 albedo_spec  : SV_Target0;
    float4 wpos_rough   : SV_Target1;
    float4 wnrm_metal   : SV_Target2;
};

shared
cbuffer FrameData : register( BSLOT( SLOT_FRAME_DATA ) )
{
    matrix _camera_world;
    matrix _viewProj;
    float _point_size;
    float3 __padding__;
};

shared
cbuffer MaterialData : register( BSLOT( SLOT_MATERIAL_DATA ) )
{
    float4 _color;
}; 

#if defined( USE_TEXTURES )
    MATERIAL_TEXTURES;
#endif

float3 GetWorldPos( in uint instanceID, in uint vertexID, in Buffer<float3> inputData, in float3x3 camera_rot )
{
    float3 pdata = inputData[instanceID];

    const float4 xoffset = float4( -1.f, 1.f, -1.f, 1.f );
    const float4 yoffset = float4( -1.f, -1.f, 1.f, 1.f );

    float3 pos_ls = float3( xoffset[vertexID], yoffset[vertexID], 0.f );
    float3 pos_ws = pdata.xyz + mul( camera_rot, pos_ls * _point_size );

    return pos_ws;
}
float2 GetTexcoord( in uint instanceID, in uint vertexID )
{
    const float4 uoffset = float4( 0.f, 1.f, 0.f, 1.f );
    const float4 voffset = float4( 0.f, 0.f, 1.f, 1.f );
    return float2( uoffset[vertexID], voffset[vertexID] );
}

in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS) 0;

    float3x3 camera_rot = ( float3x3 )_camera_world;
    float3 pos_ws = GetWorldPos( IN.instanceID, IN.vertexID, _particle_data, camera_rot );

    float4 wpos4 = float4( pos_ws, 1.0 );
    OUT.hpos = mul( _viewProj, wpos4 );
    OUT.wpos = pos_ws;
    OUT.texcoord = GetTexcoord( IN.instanceID, IN.vertexID );

    return OUT;

}

out_PS ps_main( in_PS IN )
{
    float2 uv = IN.texcoord * 2.f - 1.f;
    float len = length( uv );
    if( ( len * _point_size ) > _point_size )
        discard;
    
    float3 N;
    N.xy = uv;
    N.z = sqrt(1.f - len);
    N = normalize(N);

    float3 albedo_value = _color;
    float specular_value = 0.5f;
    float roughness_value = 0.85f;
    float metallic_value = 0.f;

    roughness_value = max( 0.01, roughness_value );

    float3 wpos = IN.wpos + N*len;

    out_PS OUT = (out_PS)0;
    OUT.albedo_spec = float4( albedo_value.rgb, specular_value );
    OUT.wpos_rough = float4( wpos, roughness_value );
    OUT.wnrm_metal = float4( N, metallic_value );

    return OUT;
}


struct in_VS_shadow
{
    uint instanceID : SV_InstanceID;
    uint vertexID : SV_VertexID;
};

struct in_PS_shadow
{
    float4 hpos	: SV_Position;
};

struct out_PS_shadow
{};

in_PS_shadow vs_shadow_main( in_VS_shadow IN )
{
    in_PS_shadow OUT = (in_PS_shadow)0;

    float3x3 camera_rot = ( float3x3 )_camera_world;
    float3 pos_ws = GetWorldPos( IN.instanceID, IN.vertexID, _particle_data, camera_rot );

    float4 wpos4 = float4( pos_ws, 1.0 );
    OUT.hpos = mul( _viewProj, wpos4 );
    
    return OUT;
}

[earlydepthstencil]
out_PS_shadow ps_shadow_main( in_PS_shadow input )
{
    out_PS_shadow OUT;
    return OUT;
}
