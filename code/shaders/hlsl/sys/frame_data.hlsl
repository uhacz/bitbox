#ifndef FRAME_DATA
#define FRAME_DATA

shared cbuffer FrameData : register(b0)
{
    matrix _camera_view;
	matrix _camera_proj;
	matrix _camera_viewProj;
	matrix _camera_world;
    float4 _camera_eyePos;
	float4 _camera_viewDir;
	//float4 _camera_projParams;
    float4 _reprojectInfo;
    float4 _reprojectInfoFromInt;
    float2 _renderTarget_rcp;
    float2 _renderTarget_size;

    float _camera_fov;
    float _camera_aspect;
    float _camera_zNear;
    float _camera_zFar;
    float _reprojectDepthScale; // (g_zFar - g_zNear) / (-g_zFar * g_zNear)
    float _reprojectDepthBias; // g_zFar / (g_zFar * g_zNear)
};

#endif
