passes:
{
    voxel1024 =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
}; #~header


/*
przesylamy tutaj index w gridzie i kolor index: 10:10:10, color: rgba
mamy tutaj tez zbindowanego mesha (box) 

na podstawie index'u obliczamy pozycje worldSpace, transformujemy wierzcholek box'a i przepisujemy kolor :)
*/

#include <sys/types.hlsl>

#define GRID_SIZE 4096
#define CELL_SIZE 0.000244140625


shared cbuffer MaterialData: register(b3)
{
    matrix _viewProj;
    matrix _world;
};

Buffer<uint2> _voxelData : register(t0);

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float4 pos	  	  : POSITION;
    float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float3 w_normal : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

struct out_PS
{  
    float4 rgba : SV_Target0;
};

uint3 getXYZ( uint index )
{
    const uint wh = GRID_SIZE*GRID_SIZE;
	const uint index_mod_wh = index % wh;
    uint3 xyz;
    xyz.x = index_mod_wh % GRID_SIZE;
	xyz.y = index_mod_wh * CELL_SIZE;
	xyz.z = index / wh;
    return xyz;
}

in_PS vs_main( in_VS input )
{
    in_PS OUT = (in_PS)0;

    const uint2 vxData = _voxelData[ input.instanceID ];

    const uint3 xyz = getXYZ( vxData.x );
    const float3 pos = (float3)xyz + 0.5f;
    
    const float3 wpos = floor( mul( _world, float4(pos,1.0 ) ).xyz ) + input.pos.xyz;
    OUT.h_pos = mul( _viewProj, float4( wpos, 1.0 ) );
    OUT.w_normal = input.normal;
    OUT.color = colorU32toFloat4_RGBA( vxData.y );
    return OUT;
}

out_PS ps_main( in_PS input )
{
    out_PS OUT;

    float3 L = normalize( float3(-1.f, 1.f, 0.f ) );
    float3 N = normalize( input.w_normal );

    const float NdotL = saturate( dot( L, N ) );

    OUT.rgba = lerp( input.color* 0.1, input.color * NdotL, NdotL );
    return OUT;
}
