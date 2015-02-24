passes:
{
    ssao = 
    {
	    vertex = "vs_screenquad";
        pixel = "ps_ssao";
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/util.hlsl>

cbuffer MaterialData : register(b3)
{
    float4 kernel[20];
    float4 kernelRadius_strength_zbias;
    float cutoff;
};

Texture2D gtex_linear_depth;
SamplerState gsamp_linear_depth;
Texture2D gtex_normal_vs;
SamplerState gsamp_normals_vs;

float _ssao( float2 uv, float2 wpos01, int num_samples )

{
	float linearDepth = gtex_linear_depth.SampleLevel( gsamp_linear_depth, uv, 0 ).x;
	float Cz = linearDepth;
    float2 winPos = 2 * wpos01 - 1;
	float3 C = resolvePositionVS( winPos, linearDepth );

	float3 N = gtex_normal_vs.SampleLevel( gsamp_normals_vs, uv, 0 ).xyz;
	N = normalize(N);

	float2 direction = kernelRadius_strength_zbias.xy;

	float sum = 0;
	for ( int i = 0; i < num_samples; ++i )
	{
		float2 coord = uv + kernel[i].xy * direction;
		float2 coordNormalized = 2 * float2(coord.x, 1-coord.y) - 1;
		float ld = gtex_linear_depth.SampleLevel( gsamp_linear_depth, coord, 0 ).x;
		float z = ld;

		float2 xy = coordNormalized * _reprojectInfo.xy * -z;
		float3 P = float3(xy, z);
		float3 V = P - C;

		float n = max( 0, dot(V, N) + Cz * kernelRadius_strength_zbias.w );
		float d = dot(V, V) + 0.0001;

		sum += n / d;

		// mirror kernel
		//
		coord = uv - kernel[i].xy * direction;
		coordNormalized = 2 * float2(coord.x, 1-coord.y) - 1;
		ld = gtex_linear_depth.SampleLevel( gsamp_linear_depth, coord, 0 ).x;
		z = ld;

		xy = coordNormalized * _reprojectInfo.xy * -z;
		P = float3(xy, z);
		V = P - C;

		n = max( 0, dot(V, N) + Cz * kernelRadius_strength_zbias.w );
		d = dot(V, V) + 0.0001;

		sum += n / d;
	}

	const float numSamplesRcp = 1.0f / float(num_samples*2);
    const float tmp = -linearDepth;
	sum *= numSamplesRcp;
	sum *= kernelRadius_strength_zbias.z + saturate( -1.0 + exp( 0.01 * tmp ) );// + (-linearDepth*0.01 );

	//if ( sum < cutoff )
    //  sum = 0;
    
    sum *= step( cutoff, sum );

	float res = max( 0, 1 - sum );
	res *= res;

	return res;
}

float ps_ssao( in out_VS_screenquad input ) : SV_Target0
{
    int num_samples = 20;
    float ssao_value = _ssao( input.uv, input.uv, num_samples );
    return ssao_value;
}