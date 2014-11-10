#ifndef FRAME_DATA
#define FRAME_DATA

shared cbuffer FrameData : register(b0)
{
    matrix view_matrix;
	matrix proj_matrix;
	matrix view_proj_matrix;
	matrix camera_world;
	float4 camera_params;
    float4 proj_params;
    float4 render_target_size_rcp;
	float4 eye_pos;
	float4 view_dir;
};

#endif
