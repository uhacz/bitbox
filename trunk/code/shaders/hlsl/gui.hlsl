passes:
{
    imgui =
    {
        vertex = "vs_imgui";
        pixel = "ps_imgui";

        hwstate = 
		{
			blend_enable = 1;
            blend_src_factor = "SRC_ALPHA";
            blend_dst_factor = "ONE_MINUS_SRC_ALPHA";
            blend_equation = "ADD";
		};
    };
};#~header

shared cbuffer MaterialData : register(b3)
{
	matrix projMatrix;
};

struct VS_INPUT
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

PS_INPUT vs_imgui(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul( projMatrix, float4(input.pos.xy, 0.f, 1.f));
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}

sampler sampler0;
Texture2D texture0;

float4 ps_imgui(PS_INPUT input) : SV_Target
{
    float4 out_col = texture0.Sample(sampler0, input.uv);
    return input.col * out_col;
}