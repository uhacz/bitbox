#ifndef UTIL_HLSL
#define UTIL_HLSL

float cameraDepth( in float3 worldPosition )
{
    const float3 inCameraSpace = worldPosition - _camera_eyePos.xyz;
	return -dot( _camera_world[2].xyz, inCameraSpace );
}

float resolveLinearDepth( float hwDepth )
{
    return rcp(hwDepth * _reprojectDepthScale + _reprojectDepthBias);
    //return -_camera_zFar * _camera_zNear / (hwDepth * (_camera_zFar - _camera_zNear) - _camera_zFar);
    //float c1 = _camera_zFar / _camera_zNear;
    //float c0 = 1.0 - c1;
    //return 1.0 / (c0 * hwDepth + c1);
}

///
// @param ab_inv : get from FrameData.proj_params.xy
float3 resolvePositionVS( float2 wpos, float linearDepth, float2 abInv )
{
    float2 xy = wpos * abInv * -linearDepth;
    return float3(xy, linearDepth);
}

#endif