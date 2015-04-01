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

//#define GRID_SIZE 32
//#define CELL_SIZE (1.0 / GRID_SIZE)


shared cbuffer MaterialData: register(b3)
{
    matrix _viewProj;
    matrix _world;
    //uint  _gridSize;
    //uint  _gridSizeSqr;
    //float _gridSizeInv;
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

int3 getXYZ( int index )
{
    //const int wh = GRID_SIZE*GRID_SIZE;
	//const int index_mod_wh = index % _gridSizeSqr;
 //   int3 xyz;
 //   xyz.x = index_mod_wh % _gridSize;
	//xyz.y = index_mod_wh / _gridSize;
	//xyz.z = index / _gridSizeSqr;
    int3 xyz;
    xyz.x = ( index >> 16 ) & 0xFF;
    xyz.y = ( index >> 8  ) & 0xFF;
    xyz.z = ( index ) & 0xFF;

    return xyz;
}

in_PS vs_main( in_VS input )
{
    in_PS OUT = (in_PS)0;

    const uint2 vxData = _voxelData[ input.instanceID ];

    const int3 xyz = getXYZ( (int)vxData.x );
    const float3 pos = (float3)xyz;
    
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
