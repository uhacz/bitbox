passes:
{
    simple =
    {
        vertex = "vs_main";
        pixel = "ps_main";
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
    float3 wnrm : TEXCOORD1;
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

in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS) 0;

    float3 pdata = _particle_data[IN.instanceID];
    
    const float4 xoffset = float4(-1.f, 1.f,-1.f, 1.f );
    const float4 yoffset = float4(-1.f, -1.f, 1.f, 1.f);
    const float4 uoffset = float4( 0.f, 1.f, 1.f, 0.f );
    const float4 voffset = float4( 0.f, 0.f, 1.f, 1.f );

    float3x3 camera_rot = ( float3x3 )_camera_world;
    
    float3 pos_ls = float3(xoffset[IN.vertexID], yoffset[IN.vertexID], 0.f);
    float3 nrm_ls = float3( 0.f, 0.f,-1.f );
    float3 pos_ws =  pdata.xyz + mul( camera_rot, pos_ls * _point_size );
    float3 nrm_ws = mul( camera_rot, nrm_ls );

    float4 wpos4 = float4( pos_ws, 1.0 );
    OUT.hpos = mul( _viewProj, wpos4 );
    OUT.wpos = pos_ws;
    OUT.wnrm = nrm_ws;
    OUT.texcoord = float2( uoffset[IN.vertexID], voffset[IN.vertexID] );

    return OUT;

}

out_PS ps_main( in_PS IN )
{
    float2 uv = IN.texcoord * 0.5f - 0.5f;
    float len = length( uv );
    if( len > _point_size )
        discard;


    float3 albedo_value = float3(1,0,0 );
    float specular_value = 0.01f;
    float roughness_value = 0.99f;
    float metallic_value = 0.f;

    roughness_value = max( 0.01, roughness_value );

    float3 N = normalize( IN.wnrm );

    out_PS OUT = (out_PS)0;
    OUT.albedo_spec = float4( albedo_value.rgb, specular_value );
    OUT.wpos_rough = float4( IN.wpos, roughness_value );
    OUT.wnrm_metal = float4( N, metallic_value );

    return OUT;
}



