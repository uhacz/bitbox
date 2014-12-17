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
    uint maxLights;
    float tileSizeRcp;

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
    float4 s_pos    : TEXCOORD0;
	float3 w_pos	: TEXCOORD1;
	/*nointerpolation*/ float3 w_normal:TEXCOORD2;
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
    
    float4 hpos = mul( view_proj_matrix, float4( world_pos.xyz, 1.f ) );
    
    output.h_pos = hpos;
	output.w_pos = world_pos.xyz;
    output.w_normal = mul( (float3x3)wmit, input.normal );
    output.s_pos = hpos;
    return output;
}

uint2 computeTileXY( in float2 uv, in float2 rtSize )
{
    const uint2 unclamped = (uint2)( ( (uv) * rtSize) / float2( tileSize, tileSize ) );
    return clamp( unclamped, uint2(0,0), uint2( numTilesX-1, numTilesY-1) );
}

out_PS ps_main( in_PS input )
{
	out_PS OUT;
    OUT.rgba = float4( 0.0, 0.0, 0.0, 0.0 );

    const float3 N = normalize( input.w_normal );
    float2 uv = (input.s_pos.xy/input.s_pos.w + 1.f) * 0.5f;
    uint2 tileXY = computeTileXY( uv, render_target_size_rcp.zw );
    uint tileIdx = ( numTilesX * tileXY.y + tileXY.x ) * maxLights;

    uint pointLightIndex = tileIdx;
    uint pointLightDataIndex = _lightsIndices[pointLightIndex] & 0xFFFF;
    while( pointLightDataIndex != 0xFFFF )
    {
        float4 lightPosRad = _lightsData[ pointLightDataIndex * 2 ];
        float4 lightColInt = _lightsData[ pointLightDataIndex * 2 + 1];
        float3 l = lightPosRad.xyz - input.w_pos;
        float d = length( l );
        float3 L = l * rcp( d );

        //float denom = 1.f - smoothstep( 0.f, lightPosRad.w, d ); // (d / lightPosRad.w) - 1.f;
        float denom = saturate(d / lightPosRad.w) - 1.f;
        float att = ( denom * denom );
        
        OUT.rgba += saturate( dot(N,L) ) * att * float4( lightColInt.xyz, 1.f );
        
        pointLightIndex++;
        pointLightDataIndex = _lightsIndices[pointLightIndex] & 0xFFFF;
    }

    float3 tmpColors[] =
    {
        float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3(0.f, 0.f, 1.f),
        float3(1.f, 1.f, 0.f), float3(0.f, 1.f, 1.f), float3(0.1f, 0.1f, 0.1f),
    };

    OUT.rgba.xyz += tmpColors[tileIdx % 6] * 0.01f;
    //OUT.rgba.xy = uv;
    //OUT.rgba.z = 0.f;
    return OUT;
}
