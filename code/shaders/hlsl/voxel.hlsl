passes:
{
    voxel =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
}; #~header


#include <sys/types.hlsl>

shared cbuffer MaterialData: register(b3)
{
    matrix _viewProj;
    matrix _world;
};

Buffer<uint> _voxelData   : register(t0);
Texture1D   _colorPalette : register(t0);
SamplerState _colorPaletteSampl : register(s0);
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
    float colorU: TEXCOORD1;
};

struct out_PS
{  
    float4 rgba : SV_Target0;
};

void voxelDecode( out int3 xyz, out uint colorIndex, int index )
{
    xyz.x = (int)( index << 24 ) >> 24;
    xyz.y = (int)( ( index << 16 ) & 0xFF000000 ) >> 24;
    xyz.z = (int)( ( index << 8  ) & 0xFF000000 ) >> 24;
    colorIndex = (uint)index >> 24;
}
#define REMAP_0xFF_01  0.00392156862745098039
in_PS vs_main( in_VS input )
{
    in_PS OUT = (in_PS)0;

    const uint vxData = _voxelData[ input.instanceID ];

    //const int3 xyz = getXYZ( (int)vxData.x );
    int3 xyz;
    uint colorIndex;
    voxelDecode( xyz, colorIndex, vxData );
    const float3 pos = (float3)xyz;
    
    const float3 wpos = round( mul( _world, float4(pos,1.0 ) ).xyz ) + input.pos.xyz;
    OUT.h_pos = mul( _viewProj, float4( wpos, 1.0 ) );
    OUT.w_normal = input.normal;
    OUT.colorU = colorIndex * REMAP_0xFF_01; // colorU32toFloat4_RGBA( vxData.y );
    return OUT;
}

out_PS ps_main( in_PS input )
{
    out_PS OUT;

    float4 color = _colorPalette.SampleLevel( _colorPaletteSampl, input.colorU, 0.0 );
    //float4 color = colorU32toFloat4_RGBA( _colorPalette.Load( input.colorIndex ) );
    float3 L = -normalize( float3(3.f, 1.f,-5.f ) );
    float3 N = normalize( input.w_normal );

    const float NdotL = saturate( dot( L, N ) );
    const float w = 0.5f;
    OUT.rgba = color * saturate( (NdotL + w) / ((1 + w) * (1 + w)) );
    return OUT;
}
