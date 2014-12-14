passes:
{
    color =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/instance_data.hlsl>
#include <sys/types.hlsl>

shared cbuffer LighningData : register(b2)
{
    uint numTilesX;
    uint numTilesY;
    uint numTiles;
    uint tileSize;
};

shared cbuffer MaterialData : register(b3)
{
	float3 diffuse_color;
	float fresnel_coeff;
	float rough_coeff;
};

Buffer<float4> _lightsData    : register(t0);
Buffer<uint>   _lightsIndices : register(t1);

struct in_VS
{
	uint   instanceID : SV_InstanceID;
	float4 pos	  	  : POSITION;
	float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float2 winpos   : TEXCOORD0;
	float3 w_pos	: TEXCOORD1;
	nointerpolation float3 w_normal:TEXCOORD2;
};

struct out_PS
{
	float4 rgba : SV_Target0;
};


in_PS vs_main( in_VS input )
{
    in_PS output;
	matrix wm = world_matrix[input.instanceID];
	matrix wmit = world_it_matrix[input.instanceID];
	float4 world_pos = mul( wm, input.pos );
    world_pos.w = 1.f;
    
    float4 hpos = mul( view_proj_matrix, world_pos );
    
    output.h_pos = hpos;
	output.w_pos = world_pos.xyz;
    output.w_normal = mul( (float3x3)wmit, input.normal );
    output.winpos = hpos.xy / hpos.w;
    return output;
}

uint2 computeTileXY( in float2 uv, in float2 rtSize )
{
    return (uint2)( (uv * rtSize) / float2( tileSize, tileSize ) );
}

out_PS ps_main( in_PS input )
{
	out_PS OUT;
    OUT.rgba = float4( 1.0, 0.0, 0.0, 0.0 );

    float2 uv = (input.winpos + 1) * 0.5f;
    //uv.y = 1.f - uv.y;
    //uv.x *= camera_params.y; // apect
    uint2 tileXY = computeTileXY( uv, render_target_size_rcp.zw );
    uint tileIdx = numTilesX * tileXY.y + tileXY.x;

    uint lIndices = _lightsIndices[tileIdx];
    if( lIndices != 0xFFFFFFFF )
    {
        OUT.rgba = float4(1.0, 1.0, 1.0, 1.0);
    }

    //float3 tmpColors[] =
    //{
    //    float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3(0.f, 0.f, 1.f),
    //    float3(1.f, 1.f, 0.f), float3(0.f, 1.f, 1.f), float3(0.1f, 0.1f, 0.1f),
    //};

    //OUT.rgba.xyz = tmpColors[tileIdx % 6];
    //OUT.rgba.xy = uv;
    //OUT.rgba.z = 0.f;
    return OUT;
}
