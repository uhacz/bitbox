#ifndef VS_SCREENQUAD
#define VS_SCREENQUAD

struct out_VS_screenquad
{
    noperspective float2 wpos01	: TEXCOORD0;
	noperspective float2 uv     : TEXCOORD1;
};

out_VS_screenquad vs_screenquad(
    in float4 IN_pos : POSITION,
    in float3 IN_uv  : TEXCOORD0, // uv and corner index
    out float4 OUT_hpos : SV_Position )
{
    OUT_hpos = float4( IN_pos.xyz, 1.0 );
    
    out_VS_screenquad output;
    output.wpos01 = IN_uv.xy;
    output.uv = IN_uv.xy;
    output.uv.y = 1 - output.uv.y;
    return output;
}

#endif