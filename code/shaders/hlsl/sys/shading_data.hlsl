#ifndef SHADING_DATA
#define SHADING_DATA

shared cbuffer ShadingData_ : register(b2)
{
	float4 sun_dir;
	float4 sun_color;
	float4 sky_params1;
	float4 sky_params2;
	float4 sky_params3;
	float4 sky_params4;
	float4 sky_params5;
	float4 sky_params6;
};

#endif